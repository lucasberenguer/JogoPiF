#ifndef ATMOSPHERE_CONFIG_H
#define ATMOSPHERE_CONFIG_H

/* Positions and areas are fractions of the map dimensions. */
enum { STEADY, PULSE, FLICKER, PHASE };
enum { LEAF, GLOW, ASH, EMBER, DUST, SMOKE, WISP };
enum { DIRECTIONAL, FROM_SOURCE };
#define LIGHT(x,y,r,rgb,a,mode) {{x,y},r,rgb,a,mode}
#define EMIT(type,x,y,w,h,count,rgb) {type,{x,y,w,h},count,rgb}
#define RAY(x,y,dx,dy,w,rgb,a) {{x,y},{dx,dy},w,rgb,a}

/* Pixel positions from the calibration sheet (1000 x 560), stored normalized. */
#define Z(x,y,w,h) {x/1000.f,y/560.f,w/1000.f,h/560.f}
#define EFFECT(x,y,w,h,c) {{x/1000.f,y/560.f},w/1000.f,h/560.f,c}
#define CANDLE(x,y,c) EFFECT(x,y,3.1f,5.f,c)
#define WARM 0xff7828
#define CANDLE_VIOLET 0xb053f5
#define TEAL 0x35d8cf

static const AtmosphereConfig configs[] = {
    [0] = {
        .vegetation={Z(4,7,174,54),Z(139,9,98,41),Z(793,8,196,66),Z(37,442,109,65),
                     Z(230,502,114,55),Z(777,438,87,44),Z(326,130,61,35),Z(615,418,54,45)},
        .gustCount=12,.gustColors={0x929d4c,0xe0bd51},
        .fog=0x71876d,.mist=.09f,.tint=0x35543b,.shadow={-.87f,.49f},
        .lights={LIGHT(.517f,.088f,.052f,0xffc75b,.13f,PULSE),
                 LIGHT(.68f,.12f,.27f,0xffd484,.13f,STEADY)},
        .rays={RAY(.682f,.012f,.41f,.86f,.044f,0xffd16d,.16f),
               RAY(.732f,.03f,.49f,.76f,.026f,0xffdc96,.11f)},
        .emitters={EMIT(LEAF,.13f,.18f,.72f,.60f,10,0xc6a84d),
                   EMIT(GLOW,.24f,.31f,.52f,.42f,8,0xffdf8a)}
    },
    [1] = {
        .vegetation={Z(7,8,113,43),Z(113,7,81,33),Z(873,9,116,51),
                     Z(844,328,127,63),Z(31,274,92,33),Z(247,425,55,40)},
        .gustCount=6,.gustColors={0xaa8068,0x9b88ab},
        .flames={CANDLE(177,136,WARM),CANDLE(183,142,WARM),CANDLE(145,219,WARM),
                 CANDLE(152,219,WARM),CANDLE(886,177,WARM),CANDLE(892,179,WARM),
                 CANDLE(107,399,WARM),CANDLE(114,399,WARM),CANDLE(242,485,WARM),
                 CANDLE(249,485,WARM),CANDLE(629,485,WARM),CANDLE(635,482,WARM)},
        .reflections={EFFECT(501,183,57,111,0xbac6ff),EFFECT(506,100,46,62,0xbac6ff)},
        .fog=0x777195,.mist=.24f,.tint=0x493e6d,.shadow={-.87f,.49f},
        .lights={LIGHT(.179f,.252f,.043f,0xffae55,.20f,FLICKER),
                 LIGHT(.151f,.399f,.042f,0xffaa63,.18f,FLICKER),
                 LIGHT(.889f,.322f,.046f,0xffac58,.22f,FLICKER),
                 LIGHT(.113f,.718f,.044f,0xffae55,.17f,FLICKER),
                 LIGHT(.245f,.872f,.049f,0xffae55,.20f,FLICKER),
                 LIGHT(.634f,.871f,.049f,0xffb267,.20f,FLICKER),
                 LIGHT(.020f,.865f,.052f,0xa95cff,.14f,PULSE),
                 LIGHT(.969f,.918f,.047f,0xad54ef,.14f,PULSE),
                 LIGHT(.196f,.078f,.041f,0xa95cff,.12f,PULSE),
                 LIGHT(.861f,.127f,.28f,0xa2b5ff,.10f,STEADY)},
        .emitters={EMIT(LEAF,.10f,.13f,.80f,.65f,9,0xb08777),
                   EMIT(WISP,.10f,.53f,.13f,.31f,5,0xa767df)}
    },
    [2] = {
        .vegetation={Z(892,4,77,48),Z(288,259,50,43),Z(720,335,53,47),Z(518,402,44,36)},
        .gustCount=5,.gustColors={0xb79276,0x91808a},
        .flames={EFFECT(946,245,18,12,WARM),CANDLE(137,206,WARM),CANDLE(142,199,WARM)},
        .reflections={EFFECT(494,179,47,105,0xe4b08b),EFFECT(746,395,33,13,0xa1a8d5),
                      EFFECT(271,429,35,12,0x9da8d5),EFFECT(550,296,28,11,0xe7a984)},
        .fog=0x88728f,.mist=.15f,.tint=0x5a405f,.shadow={-.86f,.52f},
        .lights={LIGHT(.943f,.435f,.070f,0xff8650,.21f,FLICKER),
                 LIGHT(.113f,.345f,.047f,0xffa650,.18f,FLICKER),
                 LIGHT(.102f,.721f,.041f,0xffa650,.18f,FLICKER),
                 LIGHT(.926f,.759f,.045f,0xffa650,.18f,PULSE),
                 LIGHT(.498f,.143f,.079f,0xffb45e,.19f,PULSE),
                 LIGHT(.161f,.34f,.030f,0xdf985e,.12f,PULSE),
                 LIGHT(.02f,.935f,.053f,0x9471e6,.15f,PULSE),
                 LIGHT(.972f,.934f,.052f,0x9471e6,.15f,PULSE)},
        .emitters={EMIT(SMOKE,.927f,.32f,.029f,.12f,11,0x8b6a74),
                   EMIT(SMOKE,.141f,.18f,.035f,.08f,7,0x8b7e92),
                   EMIT(SMOKE,.835f,.205f,.032f,.08f,6,0x8b7e92),
                   EMIT(EMBER,.929f,.423f,.033f,.035f,11,0xffa46a),
                   EMIT(ASH,.21f,.44f,.59f,.38f,8,0xbaa59e)}
    },
    [3] = {
        .vegetation={Z(51,484,33,37),Z(123,31,54,40),Z(160,487,41,37)},
        .flames={CANDLE(268,142,WARM),CANDLE(277,132,WARM),CANDLE(281,144,WARM),
                 CANDLE(289,142,WARM),CANDLE(410,154,WARM),CANDLE(418,150,WARM),
                 CANDLE(574,156,WARM),CANDLE(588,150,WARM),CANDLE(590,158,WARM),
                 CANDLE(716,135,WARM),CANDLE(727,135,WARM),CANDLE(35,465,WARM),
                 CANDLE(46,462,WARM),CANDLE(959,378,WARM),CANDLE(965,370,WARM),CANDLE(963,465,WARM)},
        .reflections={EFFECT(498,375,47,54,0xffc978),EFFECT(492,321,88,37,0xffce85)},
        .fog=0x7f788f,.mist=.12f,.tint=0x443b63,.shadow={.50f,.43f},.source=FROM_SOURCE,
        .lights={LIGHT(.496f,.423f,.115f,0xffc66f,.18f,PULSE),
                 LIGHT(.21f,.407f,.078f,0xa87af5,.15f,PULSE),
                 LIGHT(.273f,.27f,.038f,0xffae56,.19f,FLICKER),
                 LIGHT(.356f,.305f,.036f,0xffae56,.18f,FLICKER),
                 LIGHT(.582f,.305f,.036f,0xffae56,.18f,FLICKER),
                 LIGHT(.717f,.27f,.039f,0xffae56,.18f,FLICKER),
                 LIGHT(.948f,.71f,.041f,0xffae56,.17f,FLICKER),
                 LIGHT(.948f,.85f,.040f,0xffae56,.17f,FLICKER)},
        .rays={RAY(.496f,.12f,.496f,.54f,.11f,0xffd17d,.15f)}
    },
    [5] = {
        .vegetation={Z(13,37,31,113),Z(963,25,27,111),Z(950,489,32,45)},
        .flames={CANDLE(268,130,CANDLE_VIOLET),CANDLE(284,126,TEAL),
                 CANDLE(409,152,CANDLE_VIOLET),CANDLE(419,147,CANDLE_VIOLET),
                 CANDLE(577,150,TEAL),CANDLE(589,146,CANDLE_VIOLET),
                 CANDLE(718,126,CANDLE_VIOLET),CANDLE(731,133,TEAL),
                 CANDLE(34,461,CANDLE_VIOLET),CANDLE(42,469,CANDLE_VIOLET),CANDLE(56,477,TEAL),
                 CANDLE(954,378,CANDLE_VIOLET),CANDLE(969,373,TEAL),CANDLE(966,464,CANDLE_VIOLET),
                 CANDLE(954,471,TEAL),CANDLE(898,513,CANDLE_VIOLET)},
        .reflections={EFFECT(500,370,53,52,0xc379f4)},
        .fog=0x8b579f,.mist=.15f,.tint=0x433963,.shadow={.50f,.42f},.source=FROM_SOURCE,
        .lights={LIGHT(.497f,.439f,.114f,0xb257fc,.24f,PULSE),
                 LIGHT(.496f,.123f,.056f,0xa94a6b,.13f,FLICKER),
                 LIGHT(.274f,.257f,.038f,0xc670fc,.18f,FLICKER),
                 LIGHT(.413f,.276f,.035f,0xc670fc,.18f,FLICKER),
                 LIGHT(.594f,.28f,.034f,0xc670fc,.18f,FLICKER),
                 LIGHT(.717f,.257f,.036f,0xc670fc,.18f,FLICKER),
                 LIGHT(.032f,.846f,.043f,0x4de7c6,.16f,FLICKER),
                 LIGHT(.976f,.806f,.047f,0x4de7c6,.16f,FLICKER),
                 LIGHT(.429f,.482f,.066f,0xa355ef,.12f,PULSE)},
        .rays={RAY(.496f,.455f,.496f,.17f,.055f,0xb55aff,.20f)},
        .emitters={EMIT(WISP,.411f,.421f,.185f,.085f,14,0xca83ff)},
        .centerFog=true
    },
    [6] = {
        .vegetation={Z(65,10,69,64),Z(859,4,81,65),Z(859,371,74,60),Z(65,313,88,60)},
        .gustCount=9,.gustColors={0xb7846d,0xe16a42},
        .reflections={EFFECT(605,276,80,55,0xff7947),EFFECT(324,276,70,44,0xf85b38)},
        .fog=0x925061,.mist=.18f,.tint=0x592842,.shadow={-.86f,.52f},
        .lights={LIGHT(.61f,.506f,.07f,0xff7852,.13f,PULSE),
                 LIGHT(.32f,.514f,.04f,0xff8154,.15f,FLICKER),
                 LIGHT(.453f,.538f,.043f,0xff8154,.13f,PULSE),
                 LIGHT(.844f,.454f,.042f,0xff8154,.14f,FLICKER)},
        .rays={RAY(.926f,.026f,.641f,.297f,.052f,0xff7750,.14f)},
        .emitters={EMIT(EMBER,.15f,.32f,.65f,.45f,19,0xff8c63),
                   EMIT(ASH,.12f,.17f,.77f,.66f,16,0xa77b82)}
    },
    [7] = {
        .gustCount=7,.gustColors={0xca8274,0xff9956},
        .rays={RAY(.50f,.025f,.50f,.39f,.14f,0xff9f73,.09f)},
        .reflections={EFFECT(499,198,106,65,0xff9563),EFFECT(681,350,74,54,0xfa4f39)},
        .fog=0x884454,.mist=.17f,.tint=0x642d4c,.shadow={.50f,.02f},.source=FROM_SOURCE,
        .lights={LIGHT(.50f,.060f,.16f,0xff8b67,.22f,PULSE),
                 LIGHT(.50f,.32f,.12f,0xf98454,.11f,PULSE),
                 LIGHT(.235f,.092f,.058f,0xc67961,.10f,FLICKER),
                 LIGHT(.765f,.092f,.058f,0xc67961,.10f,FLICKER)},
        .emitters={EMIT(EMBER,.40f,.15f,.20f,.26f,19,0xff9b66),
                   EMIT(SMOKE,.43f,.23f,.14f,.23f,12,0x945553),
                   EMIT(ASH,.16f,.19f,.67f,.57f,8,0xa4666d)},
        .sky=true
    },
    [8] = {
        .gustCount=7,.gustColors={0xba828f,0xf68161},
        .rays={RAY(.50f,.17f,.50f,.42f,.16f,0xff605c,.07f)},
        .reflections={EFFECT(501,217,114,49,0xff754c),
                      EFFECT(502,340,248,125,0xf95253)},
        .fog=0x82557e,.mist=.19f,.tint=0x45294e,.shadow={.50f,.13f},.source=FROM_SOURCE,
        .lights={LIGHT(.554f,.053f,.064f,0xff405a,.19f,PULSE),
                 LIGHT(.50f,.219f,.105f,0xff765a,.22f,FLICKER),
                 LIGHT(.398f,.166f,.054f,0xf94f42,.16f,PULSE),
                 LIGHT(.606f,.166f,.054f,0xf94f42,.16f,PULSE),
                 LIGHT(.50f,.343f,.041f,0xe54a54,.17f,PHASE),
                 LIGHT(.325f,.409f,.045f,0xe54a54,.17f,PHASE),
                 LIGHT(.673f,.409f,.045f,0xe54a54,.17f,PHASE),
                 LIGHT(.50f,.711f,.047f,0xe54a54,.17f,PHASE)},
        .emitters={EMIT(EMBER,.362f,.15f,.28f,.14f,16,0xff805e),
                   EMIT(ASH,.16f,.19f,.68f,.57f,10,0xa77791),
                   EMIT(WISP,.39f,.16f,.21f,.09f,11,0xdd596e),
                   EMIT(SMOKE,.44f,.17f,.12f,.10f,9,0x97738e)},
        .sky=true
    }
};

#undef LIGHT
#undef EMIT
#undef RAY
#undef Z
#undef EFFECT
#undef CANDLE
#undef WARM
#undef CANDLE_VIOLET
#undef TEAL
#endif
