#include "logica.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned LogicFmix32(unsigned h) {
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

void LogicRngSeed(LogicRng *rng, unsigned seed) {
    if (!rng) return;
    unsigned mixed = LogicFmix32(seed);
    rng->state = mixed ? mixed : 0x6D2B79F5u;
    (void)LogicRngNext(rng);
    (void)LogicRngNext(rng);
    (void)LogicRngNext(rng);
}

unsigned LogicRngNext(LogicRng *rng) {
    unsigned x = rng->state ? rng->state : 0x6D2B79F5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x ? x : 0x6D2B79F5u;
    return rng->state;
}

int LogicRngRange(LogicRng *rng, int minVal, int maxVal) {
    if (maxVal <= minVal) return minVal;
    unsigned span = (unsigned)(maxVal - minVal) + 1u;
    unsigned r = LogicRngNext(rng);
    return minVal + (int)(((unsigned long long)r * (unsigned long long)span) >> 32);
}

int LogicCodepointCount(const char *utf8) {
    if (!utf8) return 0;
    int count = 0;
    const unsigned char *p = (const unsigned char *)utf8;
    while (*p) {
        if ((*p & 0xC0u) != 0x80u) count++;
        p++;
    }
    return count;
}

int LogicMakeVar(LogicExpr *expr, int var) {
    if (!expr || expr->count < 0 || expr->count >= LOGIC_MAX_NODES) return -1;
    if (var < 0 || var > 2) return -1;
    int idx = expr->count++;
    expr->nodes[idx] = (LogicNode){
        .kind = LOGIC_NODE_VAR,
        .var = var,
        .left = -1,
        .right = -1
    };
    expr->root = idx;
    return idx;
}

int LogicMakeNot(LogicExpr *expr, int child) {
    if (!expr || child < 0 || child >= expr->count || expr->count >= LOGIC_MAX_NODES) return -1;
    int idx = expr->count++;
    expr->nodes[idx] = (LogicNode){
        .kind = LOGIC_NODE_NOT,
        .var = 0,
        .left = child,
        .right = -1
    };
    expr->root = idx;
    return idx;
}

int LogicMakeBinary(LogicExpr *expr, LogicNodeKind kind, int left, int right) {
    if (!expr || (kind != LOGIC_NODE_AND && kind != LOGIC_NODE_OR)) return -1;
    if (left < 0 || left >= expr->count || right < 0 || right >= expr->count) return -1;
    if (expr->count >= LOGIC_MAX_NODES) return -1;
    int idx = expr->count++;
    expr->nodes[idx] = (LogicNode){
        .kind = kind,
        .var = 0,
        .left = left,
        .right = right
    };
    expr->root = idx;
    return idx;
}

static bool EvalNode(const LogicExpr *expr, int idx, const bool vars[3]) {
    if (!expr || idx < 0 || idx >= expr->count) return false;
    const LogicNode *n = &expr->nodes[idx];
    switch (n->kind) {
        case LOGIC_NODE_VAR:
            return (n->var >= 0 && n->var < 3) ? vars[n->var] : false;
        case LOGIC_NODE_NOT:
            return !EvalNode(expr, n->left, vars);
        case LOGIC_NODE_AND:
            return EvalNode(expr, n->left, vars) && EvalNode(expr, n->right, vars);
        case LOGIC_NODE_OR:
            return EvalNode(expr, n->left, vars) || EvalNode(expr, n->right, vars);
    }
    return false;
}

bool LogicEval(const LogicExpr *expr, bool p, bool q, bool r) {
    if (!expr || expr->count <= 0 || expr->root < 0 || expr->root >= expr->count) return false;
    const bool vars[3] = {p, q, r};
    return EvalNode(expr, expr->root, vars);
}

uint8_t LogicTruthMask(const LogicExpr *expr) {
    uint8_t mask = 0;
    for (int row = 0; row < 8; row++) {
        bool p = (row & 4) != 0;
        bool q = (row & 2) != 0;
        bool r = (row & 1) != 0;
        if (LogicEval(expr, p, q, r))
            mask |= (uint8_t)(1u << row);
    }
    return mask;
}

bool LogicEquivalent(const LogicExpr *a, const LogicExpr *b) {
    return LogicTruthMask(a) == LogicTruthMask(b);
}

static void CollectSubtreeInfo(const LogicExpr *expr, int idx, uint8_t *varMask, int *binOps, int *notCost) {
    if (!expr || idx < 0 || idx >= expr->count) return;
    const LogicNode *n = &expr->nodes[idx];
    switch (n->kind) {
        case LOGIC_NODE_VAR:
            if (n->var >= 0 && n->var < 3)
                *varMask |= (uint8_t)(1u << n->var);
            break;
        case LOGIC_NODE_NOT: {
            int childIdx = n->left;
            if (childIdx >= 0 && childIdx < expr->count) {
                LogicNodeKind ck = expr->nodes[childIdx].kind;
                *notCost += (ck == LOGIC_NODE_VAR) ? 1 : 2;
            }
            CollectSubtreeInfo(expr, childIdx, varMask, binOps, notCost);
            break;
        }
        case LOGIC_NODE_AND:
        case LOGIC_NODE_OR:
            (*binOps)++;
            CollectSubtreeInfo(expr, n->left, varMask, binOps, notCost);
            CollectSubtreeInfo(expr, n->right, varMask, binOps, notCost);
            break;
    }
}

uint8_t LogicUsedVarsMask(const LogicExpr *expr) {
    if (!expr || expr->count <= 0 || expr->root < 0 || expr->root >= expr->count) return 0;
    uint8_t varMask = 0;
    int binOps = 0, notCost = 0;
    CollectSubtreeInfo(expr, expr->root, &varMask, &binOps, &notCost);
    return varMask;
}

int LogicDifficultyScore(const LogicExpr *expr) {
    if (!expr || expr->count <= 0 || expr->root < 0 || expr->root >= expr->count) return 0;
    uint8_t varMask = 0;
    int binOps = 0, notCost = 0;
    CollectSubtreeInfo(expr, expr->root, &varMask, &binOps, &notCost);
    int distinctVars = ((varMask & 1u) != 0) + ((varMask & 2u) != 0) + ((varMask & 4u) != 0);
    int varPenalty = distinctVars > 0 ? (distinctVars - 1) : 0;
    return binOps + notCost + varPenalty;
}

typedef struct FormatCursor {
    char *buf;
    size_t cap;
    size_t len;
    bool ok;
} FormatCursor;

static void AppendStr(FormatCursor *cur, const char *s) {
    if (!cur->ok || !s) return;
    size_t slen = strlen(s);
    if (cur->len + slen >= cur->cap) {
        cur->ok = false;
        return;
    }
    memcpy(cur->buf + cur->len, s, slen);
    cur->len += slen;
    cur->buf[cur->len] = '\0';
}

static void FormatNode(const LogicExpr *expr, int idx, LogicStyle style, FormatCursor *cur) {
    if (!cur->ok || !expr || idx < 0 || idx >= expr->count) {
        cur->ok = false;
        return;
    }
    const LogicNode *n = &expr->nodes[idx];
    switch (n->kind) {
        case LOGIC_NODE_VAR: {
            static const char *const names[3] = {"P", "Q", "R"};
            int v = (n->var >= 0 && n->var < 3) ? n->var : 0;
            AppendStr(cur, names[v]);
            break;
        }
        case LOGIC_NODE_NOT: {
            int child = n->left;
            if (child < 0 || child >= expr->count) {
                cur->ok = false;
                return;
            }
            LogicNodeKind ck = expr->nodes[child].kind;
            bool needParens = (ck == LOGIC_NODE_AND || ck == LOGIC_NODE_OR);
            if (style == LOGIC_STYLE_SYMBOLS) {
                AppendStr(cur, "~");
                if (needParens) AppendStr(cur, "(");
                FormatNode(expr, child, style, cur);
                if (needParens) AppendStr(cur, ")");
            } else {
                AppendStr(cur, "n\xC3\xA3o ");
                if (needParens) AppendStr(cur, "(");
                FormatNode(expr, child, style, cur);
                if (needParens) AppendStr(cur, ")");
            }
            break;
        }
        case LOGIC_NODE_AND:
        case LOGIC_NODE_OR: {
            int left = n->left;
            int right = n->right;
            if (left < 0 || left >= expr->count || right < 0 || right >= expr->count) {
                cur->ok = false;
                return;
            }
            LogicNodeKind lk = expr->nodes[left].kind;
            LogicNodeKind rk = expr->nodes[right].kind;
            bool leftParens = (lk == LOGIC_NODE_AND || lk == LOGIC_NODE_OR) && lk != n->kind;
            bool rightParens = (rk == LOGIC_NODE_AND || rk == LOGIC_NODE_OR) && rk != n->kind;
            if (leftParens) AppendStr(cur, "(");
            FormatNode(expr, left, style, cur);
            if (leftParens) AppendStr(cur, ")");
            if (n->kind == LOGIC_NODE_AND)
                AppendStr(cur, style == LOGIC_STYLE_SYMBOLS ? " " LOGIC_SYM_AND " " : " e ");
            else
                AppendStr(cur, style == LOGIC_STYLE_SYMBOLS ? " " LOGIC_SYM_OR " " : " ou ");
            if (rightParens) AppendStr(cur, "(");
            FormatNode(expr, right, style, cur);
            if (rightParens) AppendStr(cur, ")");
            break;
        }
    }
}

bool LogicFormat(const LogicExpr *expr, LogicStyle style, char *buf, size_t bufSize) {
    if (!buf || bufSize == 0) return false;
    buf[0] = '\0';
    if (!expr || expr->count <= 0 || expr->root < 0 || expr->root >= expr->count) return false;
    FormatCursor cur = {.buf = buf, .cap = bufSize, .len = 0, .ok = true};
    FormatNode(expr, expr->root, style, &cur);
    if (!cur.ok) buf[0] = '\0';
    return cur.ok;
}

typedef enum TokenKind {
    TOK_EOF = 0,
    TOK_VAR,
    TOK_NOT,
    TOK_AND,
    TOK_OR,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_INVALID
} TokenKind;

typedef struct ParserState {
    const char *src;
    size_t pos;
    TokenKind tok;
    int tokVar;
    LogicExpr *expr;
    bool ok;
} ParserState;

static bool IsWordBoundary(char c) {
    unsigned char uc = (unsigned char)c;
    if (uc == 0) return true;
    if (isspace(uc) || uc == '(' || uc == ')' || uc == '~') return true;
    if (uc >= 0x80u) return false;
    return !isalnum(uc) && uc != '_';
}

static void NextToken(ParserState *ps) {
    const char *s = ps->src;
    while (s[ps->pos] && isspace((unsigned char)s[ps->pos]))
        ps->pos++;
    char c = s[ps->pos];
    if (!c) {
        ps->tok = TOK_EOF;
        return;
    }
    if (c == '(') {
        ps->pos++;
        ps->tok = TOK_LPAREN;
        return;
    }
    if (c == ')') {
        ps->pos++;
        ps->tok = TOK_RPAREN;
        return;
    }
    if (c == '~') {
        ps->pos++;
        ps->tok = TOK_NOT;
        return;
    }
    if ((unsigned char)s[ps->pos] == 0xC2u && (unsigned char)s[ps->pos + 1] == 0xACu) {
        ps->pos += 2;
        ps->tok = TOK_NOT;
        return;
    }
    if ((unsigned char)s[ps->pos] == 0xE2u && (unsigned char)s[ps->pos + 1] == 0x88u) {
        unsigned char b2 = (unsigned char)s[ps->pos + 2];
        if (b2 == 0xA7u) {
            ps->pos += 3;
            ps->tok = TOK_AND;
            return;
        }
        if (b2 == 0xA8u) {
            ps->pos += 3;
            ps->tok = TOK_OR;
            return;
        }
    }
    if ((c == 'n' || c == 'N') &&
        (unsigned char)s[ps->pos + 1] == 0xC3u &&
        ((unsigned char)s[ps->pos + 2] == 0xA3u || (unsigned char)s[ps->pos + 2] == 0x83u) &&
        (s[ps->pos + 3] == 'o' || s[ps->pos + 3] == 'O') &&
        IsWordBoundary(s[ps->pos + 4])) {
        ps->pos += 4;
        ps->tok = TOK_NOT;
        return;
    }
    if ((c == 'n' || c == 'N') &&
        (s[ps->pos + 1] == 'a' || s[ps->pos + 1] == 'A') &&
        (s[ps->pos + 2] == 'o' || s[ps->pos + 2] == 'O') &&
        IsWordBoundary(s[ps->pos + 3])) {
        ps->pos += 3;
        ps->tok = TOK_NOT;
        return;
    }
    if ((c == 'o' || c == 'O') &&
        (s[ps->pos + 1] == 'u' || s[ps->pos + 1] == 'U') &&
        IsWordBoundary(s[ps->pos + 2])) {
        ps->pos += 2;
        ps->tok = TOK_OR;
        return;
    }
    if ((c == 'e' || c == 'E') && IsWordBoundary(s[ps->pos + 1])) {
        ps->pos += 1;
        ps->tok = TOK_AND;
        return;
    }
    if ((c == 'P' || c == 'Q' || c == 'R' || c == 'A' || c == 'B' || c == 'C') &&
        IsWordBoundary(s[ps->pos + 1])) {
        if (c == 'P' || c == 'A') ps->tokVar = 0;
        else if (c == 'Q' || c == 'B') ps->tokVar = 1;
        else ps->tokVar = 2;
        ps->pos++;
        ps->tok = TOK_VAR;
        return;
    }
    ps->tok = TOK_INVALID;
    ps->ok = false;
}

static int ParseOr(ParserState *ps);

static int ParseUnary(ParserState *ps) {
    if (!ps->ok) return -1;
    if (ps->tok == TOK_NOT) {
        NextToken(ps);
        int child = ParseUnary(ps);
        if (child < 0) {
            ps->ok = false;
            return -1;
        }
        int node = LogicMakeNot(ps->expr, child);
        if (node < 0) ps->ok = false;
        return node;
    }
    if (ps->tok == TOK_VAR) {
        int v = ps->tokVar;
        NextToken(ps);
        int node = LogicMakeVar(ps->expr, v);
        if (node < 0) ps->ok = false;
        return node;
    }
    if (ps->tok == TOK_LPAREN) {
        NextToken(ps);
        int inside = ParseOr(ps);
        if (!ps->ok || inside < 0 || ps->tok != TOK_RPAREN) {
            ps->ok = false;
            return -1;
        }
        NextToken(ps);
        return inside;
    }
    ps->ok = false;
    return -1;
}

static int ParseAnd(ParserState *ps) {
    int left = ParseUnary(ps);
    while (ps->ok && ps->tok == TOK_AND) {
        NextToken(ps);
        int right = ParseUnary(ps);
        if (!ps->ok || right < 0) {
            ps->ok = false;
            return -1;
        }
        left = LogicMakeBinary(ps->expr, LOGIC_NODE_AND, left, right);
        if (left < 0) ps->ok = false;
    }
    return left;
}

static int ParseOr(ParserState *ps) {
    int left = ParseAnd(ps);
    while (ps->ok && ps->tok == TOK_OR) {
        NextToken(ps);
        int right = ParseAnd(ps);
        if (!ps->ok || right < 0) {
            ps->ok = false;
            return -1;
        }
        left = LogicMakeBinary(ps->expr, LOGIC_NODE_OR, left, right);
        if (left < 0) ps->ok = false;
    }
    return left;
}

bool LogicParse(const char *text, LogicExpr *outExpr) {
    if (!text || !outExpr) return false;
    *outExpr = (LogicExpr){.count = 0, .root = -1};
    ParserState ps = {.src = text, .pos = 0, .tok = TOK_EOF, .tokVar = 0, .expr = outExpr, .ok = true};
    NextToken(&ps);
    int root = ParseOr(&ps);
    if (!ps.ok || root < 0 || ps.tok != TOK_EOF) {
        *outExpr = (LogicExpr){.count = 0, .root = -1};
        return false;
    }
    outExpr->root = root;
    return true;
}

/* Banco de leis escritas em texto ("lado1 ≡ lado2"), lidas via LogicParse.
   Variáveis A, B, C no texto são mapeadas para 0, 1, 2 pelo parser e depois
   instanciadas com permutação de (P, Q, R) e literais opcionalmente negados. */
static const LogicLawSpec kLaws[] = {
    /* Dupla e tripla negação */
    {"~~A", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~~~A", "~A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~~(A ∧ B)", "A ∧ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~~(A ∨ B)", "A ∨ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~~(A ∧ ~B)", "A ∧ ~B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~~(~A ∨ B)", "~A ∨ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~~(A ∧ B ∧ C)", "A ∧ B ∧ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~~(A ∨ B ∨ C)", "A ∨ B ∨ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~~((A ∧ B) ∨ C)", "(A ∧ B) ∨ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},

    /* De Morgan básico (2 termos, nos dois sentidos) */
    {"~(A ∧ B)", "~A ∨ ~B", LOGIC_LAW_REWRITE, true, LOGIC_LAW_REWRITE},
    {"~(A ∨ B)", "~A ∧ ~B", LOGIC_LAW_REWRITE, true, LOGIC_LAW_REWRITE},

    /* De Morgan com dupla negação embutida (simplifica em um sentido, reescreve no outro) */
    {"~(~A ∧ B)", "A ∨ ~B", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},
    {"~(A ∧ ~B)", "~A ∨ B", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},
    {"~(~A ∨ B)", "A ∧ ~B", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},
    {"~(A ∨ ~B)", "~A ∧ B", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},
    {"~(~A ∧ ~B)", "A ∨ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~A ∨ ~B)", "A ∧ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},

    /* De Morgan com 3 termos */
    {"~(A ∧ B ∧ C)", "~A ∨ ~B ∨ ~C", LOGIC_LAW_REWRITE, true, LOGIC_LAW_REWRITE},
    {"~(A ∨ B ∨ C)", "~A ∧ ~B ∧ ~C", LOGIC_LAW_REWRITE, true, LOGIC_LAW_REWRITE},
    {"~(~A ∧ B ∧ C)", "A ∨ ~B ∨ ~C", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},
    {"~(~A ∨ B ∨ C)", "A ∧ ~B ∧ ~C", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},
    {"~(~A ∧ ~B ∧ C)", "A ∨ B ∨ ~C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~A ∨ ~B ∨ C)", "A ∧ B ∧ ~C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~A ∧ ~B ∧ ~C)", "A ∨ B ∨ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~A ∨ ~B ∨ ~C)", "A ∧ B ∧ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},

    /* De Morgan aninhado */
    {"~(A ∧ (B ∨ C))", "~A ∨ (~B ∧ ~C)", LOGIC_LAW_REWRITE, true, LOGIC_LAW_REWRITE},
    {"~(A ∨ (B ∧ C))", "~A ∧ (~B ∨ ~C)", LOGIC_LAW_REWRITE, true, LOGIC_LAW_REWRITE},
    {"~(~A ∧ (B ∨ C))", "A ∨ (~B ∧ ~C)", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},
    {"~(~A ∨ (B ∧ C))", "A ∧ (~B ∨ ~C)", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},
    {"~(A ∧ ~(B ∨ C))", "~A ∨ B ∨ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∨ ~(B ∧ C))", "~A ∧ B ∧ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~A ∧ ~(B ∨ C))", "A ∨ B ∨ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~A ∨ ~(B ∧ C))", "A ∧ B ∧ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},

    /* Idempotência e absorção */
    {"A ∧ (A ∨ B)", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"A ∨ (A ∧ B)", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~A ∧ (~A ∨ B)", "~A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~A ∨ (~A ∧ B)", "~A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∧ B) ∨ (A ∧ B ∧ C)", "A ∧ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∨ B) ∧ (A ∨ B ∨ C)", "A ∨ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"A ∧ (A ∨ B ∨ C)", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"A ∨ (A ∧ B ∧ C)", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},

    /* Complemento disfarçado */
    {"A ∧ (B ∨ ~B)", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"A ∨ (B ∧ ~B)", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∧ B) ∧ (C ∨ ~C)", "A ∧ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∨ B) ∨ (C ∧ ~C)", "A ∨ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∨ ~A) ∧ (B ∨ C)", "B ∨ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∧ ~A) ∨ (B ∧ C)", "B ∧ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},

    /* Distributiva nos dois sentidos */
    {"(A ∧ B) ∨ (A ∧ C)", "A ∧ (B ∨ C)", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},
    {"(A ∨ B) ∧ (A ∨ C)", "A ∨ (B ∧ C)", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},
    {"(A ∧ C) ∨ (B ∧ C)", "(A ∨ B) ∧ C", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},
    {"(A ∨ C) ∧ (B ∨ C)", "(A ∧ B) ∨ C", LOGIC_LAW_SIMPLIFY, true, LOGIC_LAW_REWRITE},

    /* Fatoração */
    {"(A ∧ B) ∨ (A ∧ ~B)", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∨ B) ∧ (A ∨ ~B)", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∧ B) ∨ (~A ∧ B)", "B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∨ B) ∧ (~A ∨ B)", "B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∧ B ∧ C) ∨ (A ∧ B ∧ ~C)", "A ∧ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∨ B ∨ C) ∧ (A ∨ B ∨ ~C)", "A ∨ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∧ (B ∨ C)) ∨ (A ∧ ~(B ∨ C))", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},

    /* Consenso / Absorção com negação */
    {"A ∨ (~A ∧ B)", "A ∨ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"A ∧ (~A ∨ B)", "A ∧ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~A ∨ (A ∧ B)", "~A ∨ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~A ∧ (A ∨ B)", "~A ∧ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"A ∨ (~A ∧ B ∧ C)", "A ∨ (B ∧ C)", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"A ∧ (~A ∨ B ∨ C)", "A ∧ (B ∨ C)", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},

    /* Combinações de dois passos (De Morgan + fatoração/absorção/consenso) para faixas altas (7..12) */
    {"(A ∧ B) ∨ (A ∧ ~B) ∨ (~A ∧ C)", "A ∨ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(A ∨ B) ∧ (A ∨ ~B) ∧ (~A ∨ C)", "A ∧ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∧ B) ∧ ~(A ∧ ~B)", "~A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∨ B) ∨ ~(A ∨ ~B)", "~A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∨ B) ∨ (~A ∧ B)", "~A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∧ B) ∧ (~A ∨ B)", "~A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~A ∨ B) ∨ (A ∧ B)", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~A ∧ B) ∧ (A ∨ B)", "A", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∧ B) ∧ (A ∨ C)", "(~A ∧ C) ∨ (A ∧ ~B)", LOGIC_LAW_REWRITE, false, LOGIC_LAW_REWRITE},
    {"(~(A ∧ B) ∧ A) ∨ (A ∧ C)", "A ∧ (~B ∨ C)", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(~(A ∨ B) ∨ A) ∧ (A ∨ C)", "A ∨ (~B ∧ C)", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∧ B) ∨ ~(A ∨ C)", "~A ∨ ~B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∨ B) ∧ ~(A ∧ C)", "~A ∧ ~B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"((A ∧ B) ∨ (A ∧ ~B)) ∧ (B ∨ C)", "A ∧ (B ∨ C)", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"((A ∨ B) ∧ (A ∨ ~B)) ∨ (B ∧ C)", "A ∨ (B ∧ C)", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∧ (B ∨ ~B)) ∨ (A ∧ C)", "~A ∨ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∨ (B ∧ ~B)) ∧ (A ∨ C)", "~A ∧ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~(A ∧ B) ∨ ~(A ∧ C))", "A ∧ B ∧ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~(A ∨ B) ∧ ~(A ∨ C))", "A ∨ B ∨ C", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∧ B) ∧ ~(A ∧ ~B) ∧ (B ∨ C)", "~A ∧ (B ∨ C)", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(A ∨ B) ∨ ~(A ∨ ~B) ∨ (B ∧ C)", "~A ∨ (B ∧ C)", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"(~A ∧ B ∧ C) ∨ (~A ∧ B ∧ ~C)", "~A ∧ B", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"((A ∧ B) ∨ (~A ∧ B)) ∧ (A ∨ ~C)", "B ∧ (A ∨ ~C)", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~A ∨ ~(B ∧ C)) ∨ (A ∧ ~B)", "A ∧ (~B ∨ C)", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
    {"~(~A ∧ ~(B ∨ C)) ∧ (A ∨ ~B)", "A ∨ (~B ∧ C)", LOGIC_LAW_SIMPLIFY, false, LOGIC_LAW_REWRITE},
};

int LogicLawCount(void) {
    return (int)(sizeof(kLaws) / sizeof(kLaws[0]));
}

const LogicLawSpec *LogicGetLaw(int index) {
    if (index < 0 || index >= LogicLawCount()) return NULL;
    return &kLaws[index];
}

/* ---------------- Gerador V/F ---------------- */

static int BuildRandomSubtree(LogicRng *rng, LogicExpr *expr, int numLeaves, const int varPool[3], int poolSize, bool allowGroupNot, bool allowDoubleNot) {
    if (numLeaves <= 1) {
        int v = varPool[LogicRngRange(rng, 0, poolSize - 1)];
        int node = LogicMakeVar(expr, v);
        if (node < 0) return -1;
        int roll = LogicRngRange(rng, 0, 99);
        if (allowDoubleNot && roll < 16) {
            node = LogicMakeNot(expr, node);
            if (node >= 0) node = LogicMakeNot(expr, node);
        } else if (roll < 52) {
            node = LogicMakeNot(expr, node);
        }
        return node;
    }
    int leftLeaves = LogicRngRange(rng, 1, numLeaves - 1);
    int rightLeaves = numLeaves - leftLeaves;
    int left = BuildRandomSubtree(rng, expr, leftLeaves, varPool, poolSize, allowGroupNot, allowDoubleNot);
    int right = BuildRandomSubtree(rng, expr, rightLeaves, varPool, poolSize, allowGroupNot, allowDoubleNot);
    if (left < 0 || right < 0) return -1;
    LogicNodeKind op = LogicRngRange(rng, 0, 1) == 0 ? LOGIC_NODE_AND : LOGIC_NODE_OR;
    int bin = LogicMakeBinary(expr, op, left, right);
    if (bin < 0) return -1;
    if (allowGroupNot && LogicRngRange(rng, 0, 99) < 35) {
        bin = LogicMakeNot(expr, bin);
    }
    return bin;
}

static bool PopulateVFOutput(const LogicExpr *expr, const bool vals[3], LogicStyle style, LogicVFQuestion *out) {
    char exprBuf[LOGIC_TEXT_MAX];
    if (!LogicFormat(expr, style, exprBuf, sizeof(exprBuf))) return false;
    if (LogicCodepointCount(exprBuf) > LOGIC_MAX_QUESTION_CP) return false;

    uint8_t used = LogicUsedVarsMask(expr);
    if (used == 0) return false;

    out->expr = *expr;
    out->score = LogicDifficultyScore(expr);
    for (int i = 0; i < 3; i++) {
        out->values[i] = vals[i];
        out->usesVar[i] = (used & (1u << i)) != 0;
    }
    bool evalResult = LogicEval(expr, vals[0], vals[1], vals[2]);
    out->correct = evalResult ? 0 : 1;
    snprintf(out->exprText, sizeof(out->exprText), "%s", exprBuf);

    /* Monta "P = V   Q = F" (ou com R quando R aparece, ou sempre P, Q e R se 3 variáveis) */
    char vbuf[LOGIC_TEXT_MAX];
    vbuf[0] = '\0';
    static const char *const vnames[3] = {"P", "Q", "R"};
    bool first = true;
    for (int i = 0; i < 3; i++) {
        if (!out->usesVar[i] && i == 2 && (used & 4u) == 0) continue;
        if (!out->usesVar[i] && (used != 4u)) continue;
        char piece[24];
        snprintf(piece, sizeof(piece), "%s%s = %s", first ? "" : "   ", vnames[i], vals[i] ? "V" : "F");
        size_t curLen = strlen(vbuf);
        snprintf(vbuf + curLen, sizeof(vbuf) - curLen, "%s", piece);
        first = false;
    }
    snprintf(out->valuesText, sizeof(out->valuesText), "%s", vbuf);
    snprintf(out->promptText, sizeof(out->promptText), "Qual o valor de: %.120s ?", out->exprText);
    return true;
}

static const struct {
    const char *expr;
    bool p, q, r;
} kVFFallbacks[] = {
    {"P ∧ Q", true, true, false},               /* score 2, V */
    {"P ∨ Q", false, false, false},             /* score 2, F */
    {"~~P", true, false, false},                /* score 3, V */
    {"P ∧ ~Q", true, true, false},              /* score 3, F */
    {"~(P ∧ Q)", true, false, false},           /* score 4, V */
    {"~(P ∨ Q)", true, false, false},           /* score 4, F */
    {"(P ∧ Q) ∨ (~P ∧ Q)", false, true, false}, /* score 5, V */
    {"~(P ∧ ~Q)", true, false, false},          /* score 5, F */
    {"~(P ∧ (Q ∨ R))", true, false, false},     /* score 6, V */
    {"~(P ∨ (Q ∧ R))", true, false, true}       /* score 6, F */
};

bool LogicGenerateVF(LogicRng *rng, int minScore, int maxScore, LogicStyle style, LogicVFQuestion *out) {
    if (!rng || !out) return false;
    if (maxScore < minScore) {
        int tmp = minScore;
        minScore = maxScore;
        maxScore = tmp;
    }
    int targetCorrect = LogicRngRange(rng, 0, 1); /* 0 = Verdadeiro, 1 = Falso */
    bool targetBool = (targetCorrect == 0);

    for (int attempt = 0; attempt < 320; attempt++) {
        LogicExpr expr = {.count = 0, .root = -1};
        int pattern = LogicRngRange(rng, 0, 99);
        int root = -1;
        if (minScore <= 3 && maxScore >= 3 && pattern < 14) {
            /* Caso "~~P" ou "~P ∧ Q" */
            int v = LogicRngRange(rng, 0, 1);
            int n0 = LogicMakeVar(&expr, v);
            int n1 = LogicMakeNot(&expr, n0);
            root = LogicMakeNot(&expr, n1);
        } else {
            int maxVars = (maxScore >= 4 && LogicRngRange(rng, 0, 99) < 45) ? 3 : 2;
            int varPool[3] = {0, 1, 2};
            if (maxVars == 2 && LogicRngRange(rng, 0, 99) < 25) {
                varPool[1] = 2; /* P e R */
            }
            int leaves = 2;
            if (maxScore >= 4) leaves = LogicRngRange(rng, 2, 3);
            if (maxScore >= 6) leaves = LogicRngRange(rng, 2, 4);
            bool allowGroupNot = (maxScore >= 4);
            bool allowDoubleNot = (maxScore >= 4 && LogicRngRange(rng, 0, 99) < 35);
            root = BuildRandomSubtree(rng, &expr, leaves, varPool, maxVars, allowGroupNot, allowDoubleNot);
        }
        if (root < 0) continue;
        expr.root = root;
        int score = LogicDifficultyScore(&expr);
        if (score < minScore || score > maxScore) continue;

        /* Testa as 8 atribuições de (P, Q, R) em ordem embaralhada para achar targetBool */
        int startRow = LogicRngRange(rng, 0, 7);
        for (int offset = 0; offset < 8; offset++) {
            int row = (startRow + offset) & 7;
            bool vals[3] = {(row & 4) != 0, (row & 2) != 0, (row & 1) != 0};
            if (LogicEval(&expr, vals[0], vals[1], vals[2]) == targetBool) {
                if (PopulateVFOutput(&expr, vals, style, out))
                    return true;
            }
        }
    }

    /* Fallback curado na faixa (ou mais próxima) com a resposta desejada */
    int bestIdx = 0;
    int bestDist = 999;
    int fbCount = (int)(sizeof(kVFFallbacks) / sizeof(kVFFallbacks[0]));
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < fbCount; i++) {
            LogicExpr expr;
            if (!LogicParse(kVFFallbacks[i].expr, &expr)) continue;
            bool vals[3] = {kVFFallbacks[i].p, kVFFallbacks[i].q, kVFFallbacks[i].r};
            bool ev = LogicEval(&expr, vals[0], vals[1], vals[2]);
            if (pass == 0 && ev != targetBool) continue;
            int sc = LogicDifficultyScore(&expr);
            int dist = (sc < minScore) ? (minScore - sc) : (sc > maxScore ? (sc - maxScore) : 0);
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = i;
            }
        }
        if (bestDist == 0) break;
    }
    LogicExpr fbExpr;
    LogicParse(kVFFallbacks[bestIdx].expr, &fbExpr);
    bool fbVals[3] = {kVFFallbacks[bestIdx].p, kVFFallbacks[bestIdx].q, kVFFallbacks[bestIdx].r};
    return PopulateVFOutput(&fbExpr, fbVals, style, out);
}

/* ---------------- Gerador de Múltipla Escolha ---------------- */

static const int kVarPerms[6][3] = {
    {0, 1, 2}, {0, 2, 1}, {1, 0, 2},
    {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
};

static int InstantiateNode(const LogicExpr *tmpl, int idx, const int varMap[3], const bool negMask[3], LogicExpr *out) {
    if (!tmpl || !out || idx < 0 || idx >= tmpl->count) return -1;
    const LogicNode *n = &tmpl->nodes[idx];
    switch (n->kind) {
        case LOGIC_NODE_VAR: {
            int v = (n->var >= 0 && n->var < 3) ? varMap[n->var] : 0;
            int node = LogicMakeVar(out, v);
            if (node < 0) return -1;
            if (n->var >= 0 && n->var < 3 && negMask[n->var])
                node = LogicMakeNot(out, node);
            return node;
        }
        case LOGIC_NODE_NOT: {
            int child = InstantiateNode(tmpl, n->left, varMap, negMask, out);
            return child < 0 ? -1 : LogicMakeNot(out, child);
        }
        case LOGIC_NODE_AND:
        case LOGIC_NODE_OR: {
            int l = InstantiateNode(tmpl, n->left, varMap, negMask, out);
            int r = InstantiateNode(tmpl, n->right, varMap, negMask, out);
            if (l < 0 || r < 0) return -1;
            return LogicMakeBinary(out, n->kind, l, r);
        }
    }
    return -1;
}

static bool InstantiateTemplate(const LogicExpr *tmpl, const int varMap[3], const bool negMask[3], LogicExpr *out) {
    *out = (LogicExpr){.count = 0, .root = -1};
    int root = InstantiateNode(tmpl, tmpl->root, varMap, negMask, out);
    if (root < 0) return false;
    out->root = root;
    return true;
}

/* Verifica se uma expressão cabe nos limites de codepoints nos DOIS estilos */
static bool FitsTextLimitsBothStyles(const LogicExpr *expr, int maxCodepoints) {
    char sym[LOGIC_TEXT_MAX], words[LOGIC_TEXT_MAX];
    if (!LogicFormat(expr, LOGIC_STYLE_SYMBOLS, sym, sizeof(sym))) return false;
    if (!LogicFormat(expr, LOGIC_STYLE_WORDS, words, sizeof(words))) return false;
    return LogicCodepointCount(sym) <= maxCodepoints &&
           LogicCodepointCount(words) <= maxCodepoints;
}

/* Mutações de famílias de erro para gerar candidatos a distrator a partir da resposta correta
   ou da pergunta:
   1. Trocar um operador binário (∧ <-> ∨) — erro clássico de De Morgan / distributiva
   2. Remover uma negação (~X -> X) — negação esquecida / parcial
   3. Adicionar uma negação na raiz ou num filho — negação a mais
   4. Trocar uma variável por outra presente na pergunta
   5. Negar apenas um lado de um operador binário (~A ∧ B vs ~(A ∧ B)) */
static void CollectDistractorCandidates(const LogicExpr *correct, const LogicExpr *question,
                                        LogicExpr *cands, int *candCount, int maxCands) {
    uint8_t qUsed = LogicUsedVarsMask(question);
    if (qUsed == 0) qUsed = 3u;

    /* 1. Trocar operador binário em cada nó AND/OR de `correct` */
    for (int i = 0; i < correct->count && *candCount < maxCands; i++) {
        if (correct->nodes[i].kind == LOGIC_NODE_AND || correct->nodes[i].kind == LOGIC_NODE_OR) {
            LogicExpr m = *correct;
            m.nodes[i].kind = (m.nodes[i].kind == LOGIC_NODE_AND) ? LOGIC_NODE_OR : LOGIC_NODE_AND;
            cands[(*candCount)++] = m;
        }
    }

    /* 2. Trocar variável em cada folha VAR de `correct` */
    for (int i = 0; i < correct->count && *candCount < maxCands; i++) {
        if (correct->nodes[i].kind == LOGIC_NODE_VAR) {
            for (int v = 0; v < 3 && *candCount < maxCands; v++) {
                if (v == correct->nodes[i].var) continue;
                if ((qUsed & (1u << v)) == 0 && v == 2) continue;
                LogicExpr m = *correct;
                m.nodes[i].var = v;
                cands[(*candCount)++] = m;
            }
        }
    }

    /* 3. Remover um nó NOT em `correct` (religando pai ou raiz ao filho do NOT) */
    for (int i = 0; i < correct->count && *candCount < maxCands; i++) {
        if (correct->nodes[i].kind != LOGIC_NODE_NOT) continue;
        int child = correct->nodes[i].left;
        LogicExpr m = *correct;
        if (m.root == i) {
            m.root = child;
        } else {
            for (int p = 0; p < m.count; p++) {
                if (m.nodes[p].left == i) m.nodes[p].left = child;
                if (m.nodes[p].right == i) m.nodes[p].right = child;
            }
        }
        cands[(*candCount)++] = m;
    }

    /* 4. Adicionar NOT sobre a raiz ou sobre cada subexpressão de `correct` */
    if (correct->count < LOGIC_MAX_NODES) {
        for (int i = 0; i < correct->count && *candCount < maxCands; i++) {
            LogicExpr m = *correct;
            int newNot = m.count++;
            m.nodes[newNot] = (LogicNode){.kind = LOGIC_NODE_NOT, .var = 0, .left = i, .right = -1};
            if (m.root == i) {
                m.root = newNot;
            } else {
                bool linked = false;
                for (int p = 0; p < newNot; p++) {
                    if (m.nodes[p].left == i) { m.nodes[p].left = newNot; linked = true; break; }
                    if (m.nodes[p].right == i) { m.nodes[p].right = newNot; linked = true; break; }
                }
                if (!linked) continue;
            }
            cands[(*candCount)++] = m;
        }
    }

    /* 5. Combinação: trocar operador binário + adicionar/remover NOT ou trocar variável */
    int baseCount = *candCount;
    for (int b = 0; b < baseCount && *candCount < maxCands; b++) {
        const LogicExpr *base = &cands[b];
        for (int i = 0; i < base->count && *candCount < maxCands; i++) {
            if (base->nodes[i].kind == LOGIC_NODE_AND || base->nodes[i].kind == LOGIC_NODE_OR) {
                LogicExpr m = *base;
                m.nodes[i].kind = (m.nodes[i].kind == LOGIC_NODE_AND) ? LOGIC_NODE_OR : LOGIC_NODE_AND;
                cands[(*candCount)++] = m;
            } else if (base->nodes[i].kind == LOGIC_NODE_VAR) {
                int nextVar = (base->nodes[i].var + 1) % ((qUsed & 4u) ? 3 : 2);
                LogicExpr m = *base;
                m.nodes[i].var = nextVar;
                cands[(*candCount)++] = m;
            }
        }
        if (base->count < LOGIC_MAX_NODES && *candCount < maxCands) {
            LogicExpr m = *base;
            int r = m.root;
            LogicMakeNot(&m, r);
            cands[(*candCount)++] = m;
        }
    }

    /* 6. Para respostas de 1 literal (ex.: P, ~P), incluir também binários curtos (P ∧ Q, P ∨ Q, ~P ∧ Q, ~(P ∧ Q))
       para que existam distratores de mesmo comprimento ou +1..+4 codepoints (evitando que a correta seja sempre a mais curta!) */
    static const char *const kShortPatterns[] = {
        "P", "Q", "~P", "~Q", "~~P", "~~Q",
        "P ∧ Q", "P ∨ Q", "~P ∧ Q", "~P ∨ Q", "P ∧ ~Q", "P ∨ ~Q",
        "~(P ∧ Q)", "~(P ∨ Q)", "~P ∧ ~Q", "~P ∨ ~Q",
        "P ∧ R", "P ∨ R", "Q ∧ R", "Q ∨ R", "~P ∨ R", "~Q ∨ R"
    };
    int spCount = (int)(sizeof(kShortPatterns) / sizeof(kShortPatterns[0]));
    for (int i = 0; i < spCount && *candCount < maxCands; i++) {
        LogicExpr sp;
        if (LogicParse(kShortPatterns[i], &sp)) {
            if ((LogicUsedVarsMask(&sp) & ~qUsed) == 0)
                cands[(*candCount)++] = sp;
        }
    }
}

/* Coleta distratores típicos aplicados diretamente à PERGUNTA:
   - De Morgan sem trocar operador: ~(A ∧ B) -> ~A ∧ ~B, ~(A ∨ B) -> ~A ∨ ~B
   - Negação parcial: ~(A ∧ B) -> ~A ∧ B ou A ∧ ~B
   - Remover um NOT externo ou trocar operador principal da pergunta
   - Subexpressões diretas da pergunta (filhos esquerdo/direito de binários) */
static void CollectQuestionErrorCandidates(const LogicExpr *question,
                                           LogicExpr *cands, int *candCount, int maxCands) {
    if (!question || question->count <= 0 || question->root < 0) return;

    /* 1. Subexpressões diretas (filhos) de cada nó binário ou NOT da pergunta */
    for (int i = 0; i < question->count && *candCount < maxCands; i++) {
        const LogicNode *n = &question->nodes[i];
        if (n->kind == LOGIC_NODE_AND || n->kind == LOGIC_NODE_OR) {
            LogicExpr lSub = *question; lSub.root = n->left;
            if (*candCount < maxCands) cands[(*candCount)++] = lSub;
            LogicExpr rSub = *question; rSub.root = n->right;
            if (*candCount < maxCands) cands[(*candCount)++] = rSub;

            /* Troca de operador binário na subexpressão */
            LogicExpr sw = *question; sw.root = i;
            sw.nodes[i].kind = (n->kind == LOGIC_NODE_AND) ? LOGIC_NODE_OR : LOGIC_NODE_AND;
            if (*candCount < maxCands) cands[(*candCount)++] = sw;
        } else if (n->kind == LOGIC_NODE_NOT && n->left >= 0 && n->left < question->count) {
            int child = n->left;
            const LogicNode *cn = &question->nodes[child];
            if (cn->kind == LOGIC_NODE_AND || cn->kind == LOGIC_NODE_OR) {
                /* De Morgan sem trocar operador: ~(X op Y) -> ~X op ~Y */
                if (question->count + 3 <= LOGIC_MAX_NODES && *candCount < maxCands) {
                    LogicExpr dm = *question;
                    int nl = LogicMakeNot(&dm, cn->left);
                    int nr = LogicMakeNot(&dm, cn->right);
                    int nb = LogicMakeBinary(&dm, cn->kind, nl, nr);
                    if (nb >= 0) {
                        dm.root = nb;
                        cands[(*candCount)++] = dm;
                    }
                }
                /* Negação parcial: ~(X op Y) -> ~X op Y */
                if (question->count + 2 <= LOGIC_MAX_NODES && *candCount < maxCands) {
                    LogicExpr np = *question;
                    int nl = LogicMakeNot(&np, cn->left);
                    int nb = LogicMakeBinary(&np, cn->kind, nl, cn->right);
                    if (nb >= 0) {
                        np.root = nb;
                        cands[(*candCount)++] = np;
                    }
                }
                /* Negação parcial 2: ~(X op Y) -> X op ~Y */
                if (question->count + 2 <= LOGIC_MAX_NODES && *candCount < maxCands) {
                    LogicExpr np2 = *question;
                    int nr = LogicMakeNot(&np2, cn->right);
                    int nb = LogicMakeBinary(&np2, cn->kind, cn->left, nr);
                    if (nb >= 0) {
                        np2.root = nb;
                        cands[(*candCount)++] = np2;
                    }
                }
            }
        }
    }
}

static bool PickBestValidCandidate(LogicRng *rng, const LogicExpr *cands, int candCount,
                                   uint8_t qMask, uint8_t cMask, uint8_t forbidMask,
                                   const char *qStr, const char *cStr, const char *forbidStr,
                                   int targetScore, int targetCp, LogicStyle style,
                                   LogicExpr *outExpr) {
    int validIdx[96];
    int validCost[96];
    int validCount = 0;

    for (int i = 0; i < candCount; i++) {
        uint8_t m = LogicTruthMask(&cands[i]);
        if (m == qMask || m == cMask) continue;
        if (forbidStr && m == forbidMask) continue;
        if (!FitsTextLimitsBothStyles(&cands[i], LOGIC_MAX_OPTION_CP)) continue;
        char dStr[LOGIC_TEXT_MAX];
        if (!LogicFormat(&cands[i], style, dStr, sizeof(dStr))) continue;
        if (strcmp(dStr, cStr) == 0 || strcmp(dStr, qStr) == 0) continue;
        if (forbidStr && strcmp(dStr, forbidStr) == 0) continue;

        bool dup = false;
        for (int j = 0; j < validCount; j++) {
            if (LogicTruthMask(&cands[validIdx[j]]) == m) { dup = true; break; }
            char otherStr[LOGIC_TEXT_MAX];
            LogicFormat(&cands[validIdx[j]], style, otherStr, sizeof(otherStr));
            if (strcmp(dStr, otherStr) == 0) { dup = true; break; }
        }
        if (dup) continue;

        int dScore = LogicDifficultyScore(&cands[i]);
        int dCp = LogicCodepointCount(dStr);
        int sDiff = abs(dScore - targetScore);
        int cpDiff = abs(dCp - targetCp);
        validIdx[validCount] = i;
        validCost[validCount] = (sDiff <= 1 ? 0 : (sDiff - 1) * 4) + (cpDiff <= 2 ? 0 : (cpDiff - 2) * 2);
        validCount++;
    }

    if (validCount <= 0) return false;

    int best = -1;
    int bestCost = 9999;
    int start = LogicRngRange(rng, 0, validCount - 1);
    for (int k = 0; k < validCount; k++) {
        int v = (start + k) % validCount;
        if (validCost[v] < bestCost) {
            bestCost = validCost[v];
            best = v;
        }
    }
    if (best < 0) return false;
    *outExpr = cands[validIdx[best]];
    return true;
}

static bool PickTwoDistractors(LogicRng *rng, const LogicExpr *question, const LogicExpr *correct,
                               LogicStyle style, LogicExpr outDist[2]) {
    LogicExpr cands[96];
    int candCount = 0;
    CollectDistractorCandidates(correct, question, cands, &candCount, 96);

    uint8_t qMask = LogicTruthMask(question);
    uint8_t cMask = LogicTruthMask(correct);
    char qStr[LOGIC_TEXT_MAX], cStr[LOGIC_TEXT_MAX];
    if (!LogicFormat(question, style, qStr, sizeof(qStr))) return false;
    if (!LogicFormat(correct, style, cStr, sizeof(cStr))) return false;
    int cScore = LogicDifficultyScore(correct);
    int cCp = LogicCodepointCount(cStr);

    /* Sorteia a topologia do trio (1/3 cada):
       0 = estrela (correta no centro: D1 <- C -> D2)
       1 = cadeia com D1 no meio (C -> D1 -> D2)
       2 = cadeia com D2 no meio (C -> D2 -> D1) */
    int topology = LogicRngRange(rng, 0, 2);
    /* Em ~50% das perguntas, tenta gerar pelo menos um distrator como erro típico
       aplicado diretamente à PERGUNTA (mantendo o mesmo filtro de pontuação/comprimento). */
    bool useQuestionError = (LogicRngRange(rng, 0, 1) == 1);

    LogicExpr d1;
    bool gotD1 = false;
    if (useQuestionError) {
        LogicExpr qErrCands[64];
        int qErrCount = 0;
        CollectQuestionErrorCandidates(question, qErrCands, &qErrCount, 64);
        gotD1 = PickBestValidCandidate(rng, qErrCands, qErrCount, qMask, cMask, 0,
                                       qStr, cStr, NULL, cScore, cCp, style, &d1);
    }
    if (!gotD1) {
        if (!PickBestValidCandidate(rng, cands, candCount, qMask, cMask, 0,
                                    qStr, cStr, NULL, cScore, cCp, style, &d1)) {
            return false;
        }
    }
    uint8_t d1Mask = LogicTruthMask(&d1);
    char d1Str[LOGIC_TEXT_MAX];
    LogicFormat(&d1, style, d1Str, sizeof(d1Str));

    LogicExpr d2;
    bool gotD2 = false;
    if (topology != 0) {
        LogicExpr chainCands[96];
        int chainCount = 0;
        CollectDistractorCandidates(&d1, question, chainCands, &chainCount, 96);
        gotD2 = PickBestValidCandidate(rng, chainCands, chainCount, qMask, cMask, d1Mask,
                                       qStr, cStr, d1Str, cScore, cCp, style, &d2);
    }
    if (!gotD2) {
        if (!PickBestValidCandidate(rng, cands, candCount, qMask, cMask, d1Mask,
                                    qStr, cStr, d1Str, cScore, cCp, style, &d2)) {
            return false;
        }
    }

    if (topology == 2) {
        outDist[0] = d2;
        outDist[1] = d1;
    } else {
        outDist[0] = d1;
        outDist[1] = d2;
    }
    return true;
}

static bool BuildMCFromPair(LogicRng *rng, const LogicExpr *qExpr, const LogicExpr *aExpr,
                            LogicLawType lawType, int lawIndex, LogicStyle style, LogicMCQuestion *out) {
    if (!FitsTextLimitsBothStyles(qExpr, LOGIC_MAX_QUESTION_CP)) return false;
    if (!FitsTextLimitsBothStyles(aExpr, LOGIC_MAX_OPTION_CP)) return false;

    char qStr[LOGIC_TEXT_MAX], aStr[LOGIC_TEXT_MAX];
    if (!LogicFormat(qExpr, style, qStr, sizeof(qStr))) return false;
    if (!LogicFormat(aExpr, style, aStr, sizeof(aStr))) return false;
    if (strcmp(qStr, aStr) == 0) return false;

    int qScore = LogicDifficultyScore(qExpr);
    int aScore = LogicDifficultyScore(aExpr);
    if (lawType == LOGIC_LAW_SIMPLIFY) {
        if (!(aScore < qScore)) return false;
    } else {
        if (abs(aScore - qScore) > 2) return false;
    }

    LogicExpr dist[2];
    if (!PickTwoDistractors(rng, qExpr, aExpr, style, dist)) return false;

    int correctSlot = LogicRngRange(rng, 0, 2);
    int dOrder = LogicRngRange(rng, 0, 1);
    int dIdx = 0;

    out->questionExpr = *qExpr;
    out->questionScore = qScore;
    out->lawType = lawType;
    out->lawIndex = lawIndex;
    out->correct = correctSlot;
    snprintf(out->questionText, sizeof(out->questionText), "%s", qStr);

    for (int slot = 0; slot < 3; slot++) {
        if (slot == correctSlot) {
            out->choiceExprs[slot] = *aExpr;
        } else {
            int pick = (dIdx == 0) ? dOrder : (1 - dOrder);
            out->choiceExprs[slot] = dist[pick];
            dIdx++;
        }
        out->choiceScores[slot] = LogicDifficultyScore(&out->choiceExprs[slot]);
        LogicFormat(&out->choiceExprs[slot], style, out->choices[slot], sizeof(out->choices[slot]));
    }
    return true;
}

static const struct {
    int minScore, maxScore;
    int lawIndex;
    int permIndex;
    int negBits;
    bool reverse;
} kMCFallbacks[] = {
    {4, 5, 9,  0, 0, false},  /* ~(P ∧ Q) -> ~P ∨ ~Q (score 4) */
    {4, 5, 48, 0, 0, false},  /* (P ∧ Q) ∨ (P ∧ ~Q) -> P (score 5, S) */
    {5, 6, 19, 0, 0, false},  /* ~(P ∧ (Q ∨ R)) -> ~P ∨ (~Q ∧ ~R) (score 6) */
    {6, 7, 44, 0, 0, false},  /* (P ∧ Q) ∨ (P ∧ R) -> P ∧ (Q ∨ R) (score 5..7, S) */
    {7, 8, 59, 0, 0, false},  /* ~(P ∧ Q) ∧ ~(P ∧ ~Q) -> ~P (score 8, S) */
    {8, 9, 57, 0, 0, false},  /* (P ∧ Q) ∨ (P ∧ ~Q) ∨ (~P ∧ R) -> P ∨ R (score 9, S) */
    {10, 12, 74, 0, 0, false} /* ~(P ∧ Q) ∧ ~(P ∧ ~Q) ∧ (Q ∨ R) -> ~P ∧ (Q ∨ R) (score 11, S) */
};

bool LogicGenerateMC(LogicRng *rng, int minScore, int maxScore, LogicStyle style, LogicMCQuestion *out) {
    if (!rng || !out) return false;
    if (maxScore < minScore) {
        int tmp = minScore;
        minScore = maxScore;
        maxScore = tmp;
    }

    int lawCount = LogicLawCount();
    /* Alvo de tipo: >= 60% S (usamos 72% de preferência inicial por S) */
    bool wantSimplify = (LogicRngRange(rng, 0, 99) < 74);

    for (int pass = 0; pass < 2; pass++) {
        int attempts = (pass == 0) ? 260 : 200;
        for (int attempt = 0; attempt < attempts; attempt++) {
            int lawIdx = LogicRngRange(rng, 0, lawCount - 1);
            const LogicLawSpec *law = &kLaws[lawIdx];
            bool rev = (law->bidirectional && LogicRngRange(rng, 0, 1) == 1);
            LogicLawType ltype = rev ? law->reverseType : law->forwardType;
            if (pass == 0 && (ltype == LOGIC_LAW_SIMPLIFY) != wantSimplify) continue;

            LogicExpr lhsTmpl, rhsTmpl;
            if (!LogicParse(law->lhs, &lhsTmpl) || !LogicParse(law->rhs, &rhsTmpl)) continue;
            const LogicExpr *qTmpl = rev ? &rhsTmpl : &lhsTmpl;
            const LogicExpr *aTmpl = rev ? &lhsTmpl : &rhsTmpl;

            int permIdx = LogicRngRange(rng, 0, 5);
            /* Sorteia máscara de negação dos literais A, B, C (com peso maior para 0 ou 1 literal negado) */
            int negRoll = LogicRngRange(rng, 0, 99);
            int negBits = 0;
            if (negRoll >= 45 && negRoll < 80) negBits = 1 << LogicRngRange(rng, 0, 2);
            else if (negRoll >= 80) negBits = LogicRngRange(rng, 1, 7);
            bool negMask[3] = {(negBits & 1) != 0, (negBits & 2) != 0, (negBits & 4) != 0};

            LogicExpr qExpr, aExpr;
            if (!InstantiateTemplate(qTmpl, kVarPerms[permIdx], negMask, &qExpr)) continue;
            if (!InstantiateTemplate(aTmpl, kVarPerms[permIdx], negMask, &aExpr)) continue;

            int qScore = LogicDifficultyScore(&qExpr);
            if (qScore < minScore || qScore > maxScore) continue;

            if (BuildMCFromPair(rng, &qExpr, &aExpr, ltype, lawIdx, style, out))
                return true;
        }
    }

    /* Varredura sistemática em ordem pseudoaleatória se as tentativas aleatórias não acharam */
    int startLaw = LogicRngRange(rng, 0, lawCount - 1);
    int startPerm = LogicRngRange(rng, 0, 5);
    for (int li = 0; li < lawCount; li++) {
        int lawIdx = (startLaw + li) % lawCount;
        const LogicLawSpec *law = &kLaws[lawIdx];
        LogicExpr lhsTmpl, rhsTmpl;
        if (!LogicParse(law->lhs, &lhsTmpl) || !LogicParse(law->rhs, &rhsTmpl)) continue;
        for (int dir = 0; dir < 2; dir++) {
            if (dir == 1 && !law->bidirectional) continue;
            const LogicExpr *qTmpl = dir == 0 ? &lhsTmpl : &rhsTmpl;
            const LogicExpr *aTmpl = dir == 0 ? &rhsTmpl : &lhsTmpl;
            LogicLawType ltype = dir == 0 ? law->forwardType : law->reverseType;
            for (int pi = 0; pi < 6; pi++) {
                int permIdx = (startPerm + pi) % 6;
                for (int negBits = 0; negBits < 8; negBits++) {
                    bool negMask[3] = {(negBits & 1) != 0, (negBits & 2) != 0, (negBits & 4) != 0};
                    LogicExpr qExpr, aExpr;
                    if (!InstantiateTemplate(qTmpl, kVarPerms[permIdx], negMask, &qExpr)) continue;
                    if (!InstantiateTemplate(aTmpl, kVarPerms[permIdx], negMask, &aExpr)) continue;
                    int qScore = LogicDifficultyScore(&qExpr);
                    if (qScore < minScore || qScore > maxScore) continue;
                    if (BuildMCFromPair(rng, &qExpr, &aExpr, ltype, lawIdx, style, out))
                        return true;
                }
            }
        }
    }

    /* Fallback curado mais próximo da faixa solicitada */
    int fbCount = (int)(sizeof(kMCFallbacks) / sizeof(kMCFallbacks[0]));
    int bestFb = 0;
    int bestDist = 999;
    for (int i = 0; i < fbCount; i++) {
        int mid = (kMCFallbacks[i].minScore + kMCFallbacks[i].maxScore) / 2;
        int targetMid = (minScore + maxScore) / 2;
        int d = abs(mid - targetMid);
        if (d < bestDist) {
            bestDist = d;
            bestFb = i;
        }
    }
    const LogicLawSpec *fbLaw = &kLaws[kMCFallbacks[bestFb].lawIndex];
    LogicExpr lhs, rhs, qExpr, aExpr;
    LogicParse(fbLaw->lhs, &lhs);
    LogicParse(fbLaw->rhs, &rhs);
    bool negMask[3] = {false, false, false};
    InstantiateTemplate(&lhs, kVarPerms[0], negMask, &qExpr);
    InstantiateTemplate(&rhs, kVarPerms[0], negMask, &aExpr);
    return BuildMCFromPair(rng, &qExpr, &aExpr, fbLaw->forwardType, kMCFallbacks[bestFb].lawIndex, style, out);
}
