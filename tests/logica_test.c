#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logica.h"

static void TestTruthTablesAndDeMorgan(void) {
    LogicExpr eAnd, eOr, eNot, eDeMorgan1L, eDeMorgan1R, eDeMorgan2L, eDeMorgan2R;
    assert(LogicParse("P ∧ Q", &eAnd));
    assert(LogicParse("P ∨ Q", &eOr));
    assert(LogicParse("~P", &eNot));

    for (int p = 0; p <= 1; p++) {
        for (int q = 0; q <= 1; q++) {
            for (int r = 0; r <= 1; r++) {
                assert(LogicEval(&eAnd, p != 0, q != 0, r != 0) == ((p != 0) && (q != 0)));
                assert(LogicEval(&eOr, p != 0, q != 0, r != 0) == ((p != 0) || (q != 0)));
                assert(LogicEval(&eNot, p != 0, q != 0, r != 0) == !(p != 0));
            }
        }
    }

    assert(LogicParse("~(P ∧ Q)", &eDeMorgan1L));
    assert(LogicParse("~P ∨ ~Q", &eDeMorgan1R));
    assert(LogicEquivalent(&eDeMorgan1L, &eDeMorgan1R));

    assert(LogicParse("~(P ∨ Q)", &eDeMorgan2L));
    assert(LogicParse("~P ∧ ~Q", &eDeMorgan2R));
    assert(LogicEquivalent(&eDeMorgan2L, &eDeMorgan2R));

    /* Dupla e tripla negação */
    LogicExpr eP, eNotNotP, eNotNotNotP;
    assert(LogicParse("P", &eP));
    assert(LogicParse("~~P", &eNotNotP));
    assert(LogicParse("~~~P", &eNotNotNotP));
    assert(LogicEquivalent(&eP, &eNotNotP));
    assert(LogicEquivalent(&eNot, &eNotNotNotP));
}

static void TestDifficultyScores(void) {
    /* Exemplos da especificação:
       "P ∧ Q"=2; "P ∧ ~Q"=3; "~~P"=3; "~(P ∧ Q)"=4; "(P ∧ Q) ∨ (~P ∧ Q)"=5;
       "~(P ∧ (Q ∨ R))"=6; "(P ∧ Q) ∨ (P ∧ ~Q) ∨ (~P ∧ R)"=9. */
    static const struct {
        const char *text;
        int expected;
    } cases[] = {
        {"P ∧ Q", 2},
        {"P ∧ ~Q", 3},
        {"~~P", 3},
        {"~(P ∧ Q)", 4},
        {"(P ∧ Q) ∨ (~P ∧ Q)", 5},
        {"~(P ∧ (Q ∨ R))", 6},
        {"(P ∧ Q) ∨ (P ∧ ~Q) ∨ (~P ∧ R)", 9}
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        LogicExpr expr;
        assert(LogicParse(cases[i].text, &expr));
        int score = LogicDifficultyScore(&expr);
        assert(score == cases[i].expected);
    }
}

static void TestParserAndRoundTrip(void) {
    static const char *const samples[] = {
        "P",
        "~P",
        "~~P",
        "~~~Q",
        "¬P ∧ ¬Q",
        "não (P e Q)",
        "nao P ou (nao Q e R)",
        "P ∧ Q ∨ R",
        "P ∨ Q ∧ R",
        "(P ∨ Q) ∧ R",
        "~(P ∧ (Q ∨ ~R))",
        "(P ∧ Q) ∨ (P ∧ ~Q) ∨ (~P ∧ R)"
    };
    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        LogicExpr e1, e2, e3;
        assert(LogicParse(samples[i], &e1));
        char bufSym[LOGIC_TEXT_MAX], bufWords[LOGIC_TEXT_MAX];
        assert(LogicFormat(&e1, LOGIC_STYLE_SYMBOLS, bufSym, sizeof(bufSym)));
        assert(LogicFormat(&e1, LOGIC_STYLE_WORDS, bufWords, sizeof(bufWords)));
        assert(LogicParse(bufSym, &e2));
        assert(LogicParse(bufWords, &e3));
        assert(LogicEquivalent(&e1, &e2));
        assert(LogicEquivalent(&e1, &e3));
        assert(LogicDifficultyScore(&e1) == LogicDifficultyScore(&e2));
        assert(LogicDifficultyScore(&e1) == LogicDifficultyScore(&e3));
    }

    /* Precedência: ~ > ∧ > ∨ */
    LogicExpr a, b;
    assert(LogicParse("~P ∧ Q ∨ R", &a));
    assert(LogicParse("((~P) ∧ Q) ∨ R", &b));
    assert(LogicEquivalent(&a, &b));

    LogicExpr c, d;
    assert(LogicParse("P ∨ Q ∧ ~R", &c));
    assert(LogicParse("P ∨ (Q ∧ (~R))", &d));
    assert(LogicEquivalent(&c, &d));

    /* Cadeias do mesmo operador sem parênteses */
    LogicExpr chain;
    char formatted[LOGIC_TEXT_MAX];
    assert(LogicParse("P ∨ Q ∨ R", &chain));
    assert(LogicFormat(&chain, LOGIC_STYLE_SYMBOLS, formatted, sizeof(formatted)));
    assert(strcmp(formatted, "P " LOGIC_SYM_OR " Q " LOGIC_SYM_OR " R") == 0);
}

static int InstNodeTest(const LogicExpr *tmpl, int idx, const int varMap[3], const bool negMask[3], LogicExpr *out) {
    const LogicNode *n = &tmpl->nodes[idx];
    if (n->kind == LOGIC_NODE_VAR) {
        int v = varMap[n->var];
        int node = LogicMakeVar(out, v);
        if (negMask[n->var]) node = LogicMakeNot(out, node);
        return node;
    }
    if (n->kind == LOGIC_NODE_NOT) {
        int c = InstNodeTest(tmpl, n->left, varMap, negMask, out);
        return c < 0 ? -1 : LogicMakeNot(out, c);
    }
    int l = InstNodeTest(tmpl, n->left, varMap, negMask, out);
    int r = InstNodeTest(tmpl, n->right, varMap, negMask, out);
    return (l < 0 || r < 0) ? -1 : LogicMakeBinary(out, n->kind, l, r);
}

static void TestAllLawsAndMappings(void) {
    static const int perms[6][3] = {
        {0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}
    };
    int count = LogicLawCount();
    assert(count >= 40);
    for (int i = 0; i < count; i++) {
        const LogicLawSpec *law = LogicGetLaw(i);
        assert(law != NULL);
        LogicExpr lhs, rhs;
        assert(LogicParse(law->lhs, &lhs));
        assert(LogicParse(law->rhs, &rhs));
        assert(LogicEquivalent(&lhs, &rhs));
        for (int p = 0; p < 6; p++) {
            for (int neg = 0; neg < 8; neg++) {
                bool negMask[3] = {(neg & 1) != 0, (neg & 2) != 0, (neg & 4) != 0};
                LogicExpr il = {0}, ir = {0};
                assert(InstNodeTest(&lhs, lhs.root, perms[p], negMask, &il) >= 0);
                assert(InstNodeTest(&rhs, rhs.root, perms[p], negMask, &ir) >= 0);
                assert(LogicEquivalent(&il, &ir));
            }
        }
    }
}

static int DecodeCodepointsTest(const char *s, int *out, int maxOut) {
    int n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p && n < maxOut) {
        int cp = 0;
        if ((*p & 0x80u) == 0) cp = *p++;
        else if ((*p & 0xE0u) == 0xC0u) {
            cp = (*p & 0x1Fu) << 6; p++;
            if (*p) cp |= (*p++ & 0x3Fu);
        } else if ((*p & 0xF0u) == 0xE0u) {
            cp = (*p & 0x0Fu) << 12; p++;
            if (*p) cp |= (*p++ & 0x3Fu) << 6;
            if (*p) cp |= (*p++ & 0x3Fu);
        } else cp = *p++;
        out[n++] = cp;
    }
    return n;
}

static int EditDistanceCPTest(const char *a, const char *b) {
    int cpA[128], cpB[128];
    int lenA = DecodeCodepointsTest(a, cpA, 128);
    int lenB = DecodeCodepointsTest(b, cpB, 128);
    int dp[129][129];
    for (int i = 0; i <= lenA; i++) dp[i][0] = i;
    for (int j = 0; j <= lenB; j++) dp[0][j] = j;
    for (int i = 1; i <= lenA; i++) {
        for (int j = 1; j <= lenB; j++) {
            int cost = (cpA[i - 1] == cpB[j - 1]) ? 0 : 1;
            int del = dp[i - 1][j] + 1;
            int ins = dp[i][j - 1] + 1;
            int sub = dp[i - 1][j - 1] + cost;
            int m = del < ins ? del : ins;
            dp[i][j] = m < sub ? m : sub;
        }
    }
    return dp[lenA][lenB];
}

static void CountOpsRecTest(const LogicExpr *expr, int idx, int *count) {
    if (!expr || idx < 0 || idx >= expr->count) return;
    const LogicNode *n = &expr->nodes[idx];
    if (n->kind == LOGIC_NODE_NOT) {
        (*count)++;
        CountOpsRecTest(expr, n->left, count);
    } else if (n->kind == LOGIC_NODE_AND || n->kind == LOGIC_NODE_OR) {
        (*count)++;
        CountOpsRecTest(expr, n->left, count);
        CountOpsRecTest(expr, n->right, count);
    }
}

static int CountOpsPlusNotsTest(const LogicExpr *expr) {
    int c = 0;
    CountOpsRecTest(expr, expr->root, &c);
    return c;
}

static void AddRankFractionsTest(const int m[3], int correct, double pos[3]) {
    int less = 0, eq = 0;
    int cv = m[correct];
    for (int i = 0; i < 3; i++) {
        if (m[i] < cv) less++;
        else if (m[i] == cv) eq++;
    }
    double frac = 1.0 / (double)eq;
    for (int p = less; p < less + eq; p++)
        pos[p] += frac;
}

static void TestRngEdgeSeeds(void) {
    unsigned seeds[2] = {0u, 0xFFFFFFFFu};
    for (int t = 0; t < 2; t++) {
        LogicRng rng;
        LogicRngSeed(&rng, seeds[t]);
        assert(rng.state != 0u);
        unsigned vals[16];
        for (int i = 0; i < 16; i++) {
            vals[i] = LogicRngNext(&rng);
            assert(vals[i] != 0u);
        }
        int distinct = 0;
        for (int i = 0; i < 16; i++) {
            bool seen = false;
            for (int j = 0; j < i; j++) {
                if (vals[i] == vals[j]) { seen = true; break; }
            }
            if (!seen) distinct++;
        }
        assert(distinct >= 14);
    }
}

static void TestVFGenerator(void) {
    static const int ranges[][2] = {
        {2, 3}, {3, 4}, {4, 5}, {5, 6}
    };
    for (unsigned r = 0; r < sizeof(ranges) / sizeof(ranges[0]); r++) {
        int lo = ranges[r][0], hi = ranges[r][1];
        for (int style = 0; style < 2; style++) {
            int trueCount = 0;
            int hasDoubleNot = 0;
            int hasGroupNot = 0;
            int hasThreeVars = 0;
            int N = 2000;
            for (int seed = 1; seed <= N; seed++) {
                LogicRng rng1, rng2;
                LogicRngSeed(&rng1, (unsigned)seed);
                LogicRngSeed(&rng2, (unsigned)seed);
                LogicVFQuestion q1, q2;
                assert(LogicGenerateVF(&rng1, lo, hi, (LogicStyle)style, &q1));
                assert(LogicGenerateVF(&rng2, lo, hi, (LogicStyle)style, &q2));
                /* Determinismo */
                assert(q1.correct == q2.correct);
                assert(q1.score == q2.score);
                assert(strcmp(q1.exprText, q2.exprText) == 0);
                assert(strcmp(q1.valuesText, q2.valuesText) == 0);

                /* Faixa e avaliação */
                assert(q1.score >= lo && q1.score <= hi);
                bool ev = LogicEval(&q1.expr, q1.values[0], q1.values[1], q1.values[2]);
                assert(q1.correct == (ev ? 0 : 1));
                if (q1.correct == 0) trueCount++;

                /* Limites de codepoints */
                assert(LogicCodepointCount(q1.exprText) <= LOGIC_MAX_QUESTION_CP);
                if (strstr(q1.exprText, "~~") || strstr(q1.exprText, "não não")) hasDoubleNot++;
                if (strstr(q1.exprText, "~(") || strstr(q1.exprText, "não (")) hasGroupNot++;
                if (q1.usesVar[0] && q1.usesVar[1] && q1.usesVar[2]) hasThreeVars++;
            }
            /* Sementes consecutivas 1..2000: V/F entre 46% e 54% por faixa e estilo */
            assert(trueCount >= N * 46 / 100 && trueCount <= N * 54 / 100);
            if (hi >= 3) assert(hasDoubleNot > 0);
            if (hi >= 4) {
                assert(hasGroupNot > 0);
                assert(hasThreeVars > 0);
            }
        }
    }

    /* Exercita fallback V/F com faixa impossível */
    LogicRng fbRng;
    LogicRngSeed(&fbRng, 12345u);
    LogicVFQuestion fbQ;
    assert(LogicGenerateVF(&fbRng, 99, 105, LOGIC_STYLE_SYMBOLS, &fbQ));
    bool fbEv = LogicEval(&fbQ.expr, fbQ.values[0], fbQ.values[1], fbQ.values[2]);
    assert(fbQ.correct == (fbEv ? 0 : 1));
}

static void TestMCGeneratorAndStatistics(void) {
    static const int ranges[][2] = {
        {4, 5}, {5, 6}, {6, 7}, {7, 8}, {8, 9}, {10, 12}
    };
    int N = 800;
    const char *envN = getenv("LOGICA_TEST_N");
    if (envN && atoi(envN) >= 200) N = atoi(envN);

    for (unsigned r = 0; r < sizeof(ranges) / sizeof(ranges[0]); r++) {
        int lo = ranges[r][0], hi = ranges[r][1];
        for (int style = 0; style < 2; style++) {
            int slotCounts[3] = {0, 0, 0};
            int simplifyCount = 0;
            int shortestCorrectCount = 0;
            int hasDoubleNot = 0;
            int hasDeMorganAnd = 0;
            int hasDeMorganOr = 0;
            double rankPos[5][3] = {{0}};

            for (int seed = 1; seed <= N; seed++) {
                LogicRng rng1, rng2;
                unsigned s = (unsigned)(seed * 131 + r * 31 + style * 7);
                LogicRngSeed(&rng1, s);
                LogicRngSeed(&rng2, s);

                LogicMCQuestion q1, q2;
                assert(LogicGenerateMC(&rng1, lo, hi, (LogicStyle)style, &q1));
                assert(LogicGenerateMC(&rng2, lo, hi, (LogicStyle)style, &q2));

                /* Determinismo */
                assert(q1.correct == q2.correct);
                assert(q1.questionScore == q2.questionScore);
                assert(strcmp(q1.questionText, q2.questionText) == 0);
                for (int c = 0; c < 3; c++)
                    assert(strcmp(q1.choices[c], q2.choices[c]) == 0);

                /* Pontuação dentro da faixa */
                assert(q1.questionScore >= lo && q1.questionScore <= hi);
                assert(q1.correct >= 0 && q1.correct < 3);
                slotCounts[q1.correct]++;

                /* Limites de codepoints nos dois estilos */
                assert(LogicCodepointCount(q1.questionText) <= LOGIC_MAX_QUESTION_CP);
                for (int c = 0; c < 3; c++)
                    assert(LogicCodepointCount(q1.choices[c]) <= LOGIC_MAX_OPTION_CP);

                /* Equivalência da correta e NÃO equivalência dos distratores */
                uint8_t qMask = LogicTruthMask(&q1.questionExpr);
                uint8_t cMasks[3];
                int cps[3];
                for (int c = 0; c < 3; c++) {
                    cMasks[c] = LogicTruthMask(&q1.choiceExprs[c]);
                    cps[c] = LogicCodepointCount(q1.choices[c]);
                    if (c == q1.correct) {
                        assert(cMasks[c] == qMask);
                    } else {
                        assert(cMasks[c] != qMask);
                    }
                }
                /* Distratores distintos entre si e da correta (por máscara e por texto) */
                assert(cMasks[0] != cMasks[1] && cMasks[0] != cMasks[2] && cMasks[1] != cMasks[2]);
                assert(strcmp(q1.choices[0], q1.choices[1]) != 0);
                assert(strcmp(q1.choices[0], q1.choices[2]) != 0);
                assert(strcmp(q1.choices[1], q1.choices[2]) != 0);

                /* Tipo S/R respeitado */
                int cScore = q1.choiceScores[q1.correct];
                if (q1.lawType == LOGIC_LAW_SIMPLIFY) {
                    assert(cScore < q1.questionScore);
                    simplifyCount++;
                } else {
                    assert(abs(cScore - q1.questionScore) <= 2);
                    assert(strcmp(q1.questionText, q1.choices[q1.correct]) != 0);
                }

                /* Verifica se a correta é estritamente a mais curta entre as 3 opções */
                int d1 = (q1.correct + 1) % 3;
                int d2 = (q1.correct + 2) % 3;
                if (cps[q1.correct] < cps[d1] && cps[q1.correct] < cps[d2])
                    shortestCorrectCount++;

                /* 5 métricas de posição (1º, 2º, 3º) */
                int metrics[5][3];
                int dOpt[3][3] = {{0}};
                for (int i = 0; i < 3; i++) {
                    for (int j = i + 1; j < 3; j++) {
                        int d = EditDistanceCPTest(q1.choices[i], q1.choices[j]);
                        dOpt[i][j] = dOpt[j][i] = d;
                    }
                }
                for (int i = 0; i < 3; i++) {
                    metrics[0][i] = q1.choiceScores[i];
                    metrics[1][i] = CountOpsPlusNotsTest(&q1.choiceExprs[i]);
                    metrics[2][i] = dOpt[i][(i + 1) % 3] + dOpt[i][(i + 2) % 3];
                    metrics[3][i] = EditDistanceCPTest(q1.choices[i], q1.questionText);
                    metrics[4][i] = cps[i];
                }
                for (int k = 0; k < 5; k++)
                    AddRankFractionsTest(metrics[k], q1.correct, rankPos[k]);

                /* Presença de ~~ e De Morgan */
                char symQ[LOGIC_TEXT_MAX];
                LogicFormat(&q1.questionExpr, LOGIC_STYLE_SYMBOLS, symQ, sizeof(symQ));
                if (strstr(symQ, "~~")) hasDoubleNot++;
                if (strstr(symQ, "~(") && strstr(symQ, LOGIC_SYM_AND)) hasDeMorganAnd++;
                if (strstr(symQ, "~(") && strstr(symQ, LOGIC_SYM_OR)) hasDeMorganOr++;
            }

            assert(shortestCorrectCount <= N * 50 / 100);
            for (int c = 0; c < 3; c++) {
                assert(slotCounts[c] >= N * 28 / 100 && slotCounts[c] <= N * 38 / 100);
            }
            assert(simplifyCount >= N * 60 / 100);
            assert(hasDoubleNot > 0);
            assert(hasDeMorganAnd > 0);
            assert(hasDeMorganOr > 0);

            /* Critério permanente: cada posição (1º, 2º, 3º) das 5 métricas entre 20% e 45% */
            for (int k = 0; k < 5; k++) {
                for (int p = 0; p < 3; p++) {
                    double pct = 100.0 * rankPos[k][p] / (double)N;
                    assert(pct >= 20.0 && pct <= 45.0);
                }
            }
        }
    }

    /* Exercita fallback MC com faixa impossível */
    LogicRng fbRng;
    LogicRngSeed(&fbRng, 54321u);
    LogicMCQuestion fbMC;
    assert(LogicGenerateMC(&fbRng, 99, 105, LOGIC_STYLE_SYMBOLS, &fbMC));
    assert(LogicEquivalent(&fbMC.questionExpr, &fbMC.choiceExprs[fbMC.correct]));
}

int main(void) {
    TestRngEdgeSeeds();
    TestTruthTablesAndDeMorgan();
    TestDifficultyScores();
    TestParserAndRoundTrip();
    TestAllLawsAndMappings();
    TestVFGenerator();
    TestMCGeneratorAndStatistics();
    puts("All propositional logic tests passed.");
    return 0;
}
