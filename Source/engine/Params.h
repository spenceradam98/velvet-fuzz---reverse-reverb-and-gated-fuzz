// The parameter table: the single source of truth for the engine, the plugin's
// AudioProcessorValueTreeState, the presets and the editor.
#pragma once

namespace vf
{
enum class Kind { Float, Choice, Bool };

struct ParamDef
{
    const char* id;
    const char* name;
    Kind kind;
    float min, max, def;
    float centre;          // value shown at 12 o'clock for skewed ranges (0 = linear)
    const char* unit;
    const char* choices;   // '|' separated, for Choice
};

// X(enumName, id, display name, kind, min, max, default, centre, unit, choices)
#define VF_PARAMS(X) \
    X (Order,      "order",      "Order",            Choice, 0, 2, 0, 0, "", "Verb > Fuzz|Fuzz > Verb|Parallel") \
    X (VerbOn,     "verb_on",    "Reverse On",       Bool,   0, 1, 1, 0, "", "") \
    X (VerbMode,   "verb_mode",  "Reverse Mode",     Choice, 0, 1, 0, 0, "", "Gated Reverse|Tape Reverse") \
    X (VerbLen,    "verb_len",   "Length",           Float, 100, 2000, 600, 500, "ms", "") \
    X (VerbSync,   "verb_sync",  "Length Sync",      Choice, 0, 7, 0, 0, "", "Free|1/16|1/8|1/8D|1/4|1/4D|1/2|1/1") \
    X (VerbSwell,  "verb_swell", "Swell",            Float,  0, 1, 0.5f, 0, "", "") \
    X (VerbSize,   "verb_size",  "Size",             Float,  0, 1, 0.6f, 0, "", "") \
    X (VerbDamp,   "verb_damp",  "Damping",          Float,  0, 1, 0.35f, 0, "", "") \
    X (VerbPre,    "verb_pre",   "Pre-delay",        Float,  0, 250, 0, 40, "ms", "") \
    X (VerbMix,    "verb_mix",   "Reverse Mix",      Float,  0, 1, 0.5f, 0, "", "") \
    X (FuzzOn,     "fuzz_on",    "Fuzz On",          Bool,   0, 1, 1, 0, "", "") \
    X (FuzzGain,   "fuzz_gain",  "Gain",             Float,  0, 1, 0.6f, 0, "", "") \
    X (FuzzBias,   "fuzz_bias",  "Bias",             Float,  0, 1, 0.75f, 0, "", "") \
    X (FuzzSag,    "fuzz_sag",   "Sag",              Float,  0, 1, 0.2f, 0, "", "") \
    X (FuzzGate,   "fuzz_gate",  "Gate",             Float,  0, 1, 0.15f, 0, "", "") \
    X (FuzzTone,   "fuzz_tone",  "Tone",             Float,  0, 1, 0.5f, 0, "", "") \
    X (FuzzMids,   "fuzz_mids",  "Mids",             Float, -1, 1, 0, 0, "mids", "") \
    X (FuzzLevel,  "fuzz_level", "Fuzz Level",       Float, -24, 12, 0, 0, "dB", "") \
    X (Width,      "width",      "Width",            Float,  0, 2, 1, 0, "", "") \
    X (InTrim,     "in_trim",    "Input Trim",       Float, -24, 24, 0, 0, "dB", "") \
    X (OutGain,    "out_gain",   "Output",           Float, -24, 12, 0, 0, "dB", "") \
    X (Oversample, "os",         "Oversampling",     Choice, 0, 1, 0, 0, "", "2x|4x") \
    X (Bypass,     "bypass",     "Bypass",           Bool,   0, 1, 0, 0, "", "")

enum P : int
{
#define VF_ENUM(e, ...) e,
    VF_PARAMS (VF_ENUM)
#undef VF_ENUM
    NumParams
};

inline const ParamDef* paramDefs()
{
#define VF_DEF(e, id, name, kind, mn, mx, df, ct, unit, ch) ParamDef { id, name, Kind::kind, (float) (mn), (float) (mx), (float) (df), (float) (ct), unit, ch },
    static const ParamDef defs[NumParams] = { VF_PARAMS (VF_DEF) };
#undef VF_DEF
    return defs;
}

// Index for an id string, or -1.
int paramIndex (const char* id);

// Beats for the Length Sync choices (index 0 = free).
inline double syncBeats (int index)
{
    static const double beats[] = { 0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 4.0 };
    return (index > 0 && index < 8) ? beats[index] : 0.0;
}
} // namespace vf
