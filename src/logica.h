#ifndef LOGICA_H
#define LOGICA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOGIC_MAX_NODES 32
#define LOGIC_TEXT_MAX 160
#define LOGIC_CHOICE_COUNT 3
#define LOGIC_MAX_QUESTION_CP 44
#define LOGIC_MAX_OPTION_CP 32

#define LOGIC_SYM_AND "\xE2\x88\xA7"
#define LOGIC_SYM_OR  "\xE2\x88\xA8"

typedef enum LogicNodeKind {
    LOGIC_NODE_VAR = 0,
    LOGIC_NODE_NOT,
    LOGIC_NODE_AND,
    LOGIC_NODE_OR
} LogicNodeKind;

typedef enum LogicStyle {
    LOGIC_STYLE_SYMBOLS = 0,
    LOGIC_STYLE_WORDS
} LogicStyle;

typedef enum LogicLawType {
    LOGIC_LAW_SIMPLIFY = 0, /* S: correta pontua estritamente menos que a pergunta */
    LOGIC_LAW_REWRITE       /* R: |diferenca| <= 2 e texto diferente */
} LogicLawType;

typedef struct LogicNode {
    LogicNodeKind kind;
    int var;   /* 0=P, 1=Q, 2=R para LOGIC_NODE_VAR */
    int left;  /* filho unico para NOT, filho esquerdo para AND/OR */
    int right; /* -1 para VAR/NOT, filho direito para AND/OR */
} LogicNode;

typedef struct LogicExpr {
    LogicNode nodes[LOGIC_MAX_NODES];
    int count;
    int root;
} LogicExpr;

typedef struct LogicRng {
    unsigned state;
} LogicRng;

typedef struct LogicVFQuestion {
    LogicExpr expr;
    bool values[3]; /* P, Q, R */
    bool usesVar[3];
    int score;
    int correct; /* 0 = Verdadeiro, 1 = Falso */
    char exprText[LOGIC_TEXT_MAX];
    char valuesText[LOGIC_TEXT_MAX];
    char promptText[LOGIC_TEXT_MAX];
} LogicVFQuestion;

typedef struct LogicMCQuestion {
    LogicExpr questionExpr;
    LogicExpr choiceExprs[LOGIC_CHOICE_COUNT];
    int questionScore;
    int choiceScores[LOGIC_CHOICE_COUNT];
    LogicLawType lawType;
    int lawIndex;
    int correct; /* 0..2 */
    char questionText[LOGIC_TEXT_MAX];
    char choices[LOGIC_CHOICE_COUNT][LOGIC_TEXT_MAX];
} LogicMCQuestion;

typedef struct LogicSealQuestion {
    bool isBoss;
    int correct; /* V/F: 0=Verdadeiro, 1=Falso; Boss: 0..2 */
    int score;
    LogicVFQuestion vf;
    LogicMCQuestion mc;
} LogicSealQuestion;

typedef struct LogicLawSpec {
    const char *lhs;
    const char *rhs;
    LogicLawType forwardType;
    bool bidirectional;
    LogicLawType reverseType;
} LogicLawSpec;

void LogicRngSeed(LogicRng *rng, unsigned seed);
unsigned LogicRngNext(LogicRng *rng);
int LogicRngRange(LogicRng *rng, int minVal, int maxVal);

int LogicCodepointCount(const char *utf8);

int LogicMakeVar(LogicExpr *expr, int var);
int LogicMakeNot(LogicExpr *expr, int child);
int LogicMakeBinary(LogicExpr *expr, LogicNodeKind kind, int left, int right);

bool LogicEval(const LogicExpr *expr, bool p, bool q, bool r);
uint8_t LogicTruthMask(const LogicExpr *expr);
bool LogicEquivalent(const LogicExpr *a, const LogicExpr *b);
uint8_t LogicUsedVarsMask(const LogicExpr *expr);
int LogicDifficultyScore(const LogicExpr *expr);

bool LogicParse(const char *text, LogicExpr *outExpr);
bool LogicFormat(const LogicExpr *expr, LogicStyle style, char *buf, size_t bufSize);

int LogicLawCount(void);
const LogicLawSpec *LogicGetLaw(int index);

bool LogicGenerateVF(LogicRng *rng, int minScore, int maxScore, LogicStyle style, LogicVFQuestion *out);
bool LogicGenerateMC(LogicRng *rng, int minScore, int maxScore, LogicStyle style, LogicMCQuestion *out);

#endif
