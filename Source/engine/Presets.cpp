// Factory presets. Each preset lists only what differs from the parameter defaults.
//
// Choice indices, for reference:
//   order:     0 Verb > Fuzz, 1 Fuzz > Verb, 2 Parallel
//   verb_mode: 0 Gated Reverse, 1 Tape Reverse
//   verb_sync: 0 Free, 1 1/16, 2 1/8, 3 1/8D, 4 1/4, 5 1/4D, 6 1/2, 7 1/1
//   os:        0 2x, 1 4x
#include "Engine.h"

namespace vf
{
namespace
{
const char* const kGlide = "Glide";
const char* const kWall = "Wall";
const char* const kReverse = "Reverse";
const char* const kFuzz = "Fuzz";
const char* const kUtil = "Parallel & Utility";

std::vector<Preset> build()
{
    return {
        // ------------------------------------------------------------ Glide
        { "Glide Guitar", kGlide, { {"order",0}, {"verb_len",700}, {"verb_swell",0.55f}, {"verb_size",0.7f}, {"verb_mix",0.55f},
                                    {"fuzz_gain",0.45f}, {"fuzz_bias",0.8f}, {"fuzz_sag",0.15f}, {"fuzz_tone",0.42f}, {"fuzz_mids",-0.2f}, {"width",1.3f} } },
        { "Tremolo Arm Bloom", kGlide, { {"order",0}, {"verb_len",900}, {"verb_swell",0.7f}, {"verb_size",0.8f}, {"verb_damp",0.5f}, {"verb_mix",0.6f},
                                         {"fuzz_gain",0.35f}, {"fuzz_bias",0.85f}, {"fuzz_tone",0.38f}, {"width",1.4f} } },
        { "Soft Focus", kGlide, { {"order",1}, {"verb_len",500}, {"verb_swell",0.45f}, {"verb_size",0.6f}, {"verb_damp",0.55f}, {"verb_mix",0.45f},
                                  {"fuzz_gain",0.25f}, {"fuzz_bias",0.9f}, {"fuzz_tone",0.35f}, {"fuzz_level",-3} } },
        { "Sitar Swell", kGlide, { {"order",1}, {"verb_len",350}, {"verb_swell",0.8f}, {"verb_size",0.35f}, {"verb_damp",0.2f}, {"verb_mix",0.5f},
                                   {"fuzz_gain",0.55f}, {"fuzz_bias",0.6f}, {"fuzz_tone",0.75f}, {"fuzz_mids",0.6f} } },
        { "Quarter Note Glide", kGlide, { {"order",0}, {"verb_sync",4}, {"verb_swell",0.6f}, {"verb_size",0.7f}, {"verb_mix",0.55f},
                                          {"fuzz_gain",0.5f}, {"fuzz_bias",0.8f}, {"fuzz_tone",0.45f}, {"width",1.3f} } },

        // ------------------------------------------------------------ Wall
        { "Loveless Wall", kWall, { {"order",0}, {"verb_len",1200}, {"verb_swell",0.5f}, {"verb_size",0.9f}, {"verb_damp",0.45f}, {"verb_mix",0.7f},
                                    {"fuzz_gain",0.85f}, {"fuzz_bias",0.7f}, {"fuzz_sag",0.3f}, {"fuzz_tone",0.4f}, {"fuzz_mids",-0.5f}, {"width",1.5f}, {"out_gain",-3} } },
        { "Noise Pop Smear", kWall, { {"order",0}, {"verb_mode",1}, {"verb_len",800}, {"verb_swell",0.4f}, {"verb_size",0.8f}, {"verb_mix",0.6f},
                                      {"fuzz_gain",0.9f}, {"fuzz_bias",0.65f}, {"fuzz_tone",0.5f}, {"fuzz_mids",-0.35f}, {"width",1.4f}, {"out_gain",-4} } },
        { "Cathedral Of Static", kWall, { {"order",0}, {"verb_len",2000}, {"verb_swell",0.35f}, {"verb_size",1.0f}, {"verb_damp",0.6f}, {"verb_mix",0.8f},
                                          {"fuzz_gain",1.0f}, {"fuzz_bias",0.7f}, {"fuzz_sag",0.25f}, {"fuzz_tone",0.3f}, {"width",1.6f}, {"out_gain",-5} } },
        { "Synth Smear", kWall, { {"order",0}, {"verb_mode",1}, {"verb_len",1000}, {"verb_swell",0.5f}, {"verb_size",0.75f}, {"verb_damp",0.3f}, {"verb_mix",0.55f},
                                  {"fuzz_gain",0.6f}, {"fuzz_bias",0.75f}, {"fuzz_tone",0.55f}, {"fuzz_mids",0.2f}, {"os",1}, {"width",1.3f} } },
        { "Feedback Halo", kWall, { {"order",1}, {"verb_len",1500}, {"verb_swell",0.3f}, {"verb_size",0.95f}, {"verb_mix",0.65f},
                                    {"fuzz_gain",0.95f}, {"fuzz_bias",0.75f}, {"fuzz_sag",0.4f}, {"fuzz_tone",0.5f}, {"fuzz_mids",-0.6f}, {"width",1.5f}, {"out_gain",-4} } },

        // ------------------------------------------------------------ Reverse
        { "Backwards Halo", kReverse, { {"order",0}, {"fuzz_on",0}, {"verb_mode",1}, {"verb_len",900}, {"verb_swell",0.6f}, {"verb_size",0.8f}, {"verb_mix",0.55f}, {"width",1.3f} } },
        { "Reverse Pad Only", kReverse, { {"order",0}, {"fuzz_on",0}, {"verb_mode",1}, {"verb_len",1600}, {"verb_swell",0.4f}, {"verb_size",0.9f}, {"verb_damp",0.5f}, {"verb_mix",1.0f}, {"width",1.5f} } },
        { "Gated 80s Rack", kReverse, { {"fuzz_on",0}, {"verb_len",450}, {"verb_swell",0.75f}, {"verb_size",0.55f}, {"verb_damp",0.25f}, {"verb_mix",0.45f} } },
        { "Inhale", kReverse, { {"fuzz_on",0}, {"verb_len",300}, {"verb_swell",0.9f}, {"verb_size",0.4f}, {"verb_pre",20}, {"verb_mix",0.5f} } },
        { "Tape Flip Eighths", kReverse, { {"fuzz_on",0}, {"verb_mode",1}, {"verb_sync",2}, {"verb_swell",0.55f}, {"verb_size",0.6f}, {"verb_mix",0.5f} } },

        // ------------------------------------------------------------ Fuzz
        { "Dying Battery", kFuzz, { {"order",1}, {"verb_len",600}, {"verb_mix",0.3f},
                                    {"fuzz_gain",0.7f}, {"fuzz_bias",0.4f}, {"fuzz_sag",0.9f}, {"fuzz_gate",0.25f}, {"fuzz_tone",0.45f} } },
        { "Velcro Splat", kFuzz, { {"order",1}, {"verb_len",400}, {"verb_swell",0.7f}, {"verb_mix",0.35f},
                                   {"fuzz_gain",0.8f}, {"fuzz_bias",0.15f}, {"fuzz_sag",0.5f}, {"fuzz_gate",0.4f}, {"fuzz_tone",0.6f}, {"fuzz_mids",0.3f}, {"fuzz_level",3} } },
        { "Starved Bass", kFuzz, { {"order",1}, {"verb_on",0}, {"fuzz_gain",0.6f}, {"fuzz_bias",0.3f}, {"fuzz_sag",0.6f}, {"fuzz_gate",0.3f},
                                   {"fuzz_tone",0.3f}, {"fuzz_mids",0.25f}, {"width",0.6f} } },
        { "Splatter Gate", kFuzz, { {"order",1}, {"verb_len",250}, {"verb_swell",0.85f}, {"verb_mix",0.4f},
                                    {"fuzz_gain",1.0f}, {"fuzz_bias",0.25f}, {"fuzz_gate",0.7f}, {"fuzz_tone",0.55f}, {"fuzz_mids",-0.3f} } },
        { "Fuzz Only", kFuzz, { {"verb_on",0}, {"fuzz_gain",0.7f}, {"fuzz_bias",0.75f}, {"fuzz_sag",0.2f}, {"fuzz_gate",0.15f}, {"fuzz_tone",0.5f}, {"fuzz_mids",-0.3f} } },

        // ------------------------------------------------------------ Parallel & Utility
        { "Parallel Haze", kUtil, { {"order",2}, {"verb_mode",1}, {"verb_len",1100}, {"verb_swell",0.5f}, {"verb_size",0.8f}, {"verb_mix",0.6f},
                                    {"fuzz_gain",0.55f}, {"fuzz_bias",0.8f}, {"fuzz_tone",0.45f}, {"width",1.4f} } },
        { "Clean Under Fuzz", kUtil, { {"order",2}, {"verb_len",700}, {"verb_swell",0.55f}, {"verb_mix",0.5f},
                                       {"fuzz_gain",0.75f}, {"fuzz_bias",0.55f}, {"fuzz_gate",0.3f}, {"fuzz_tone",0.5f}, {"fuzz_level",-4} } },
        { "Vocal Reverse Throw", kUtil, { {"order",2}, {"fuzz_on",0}, {"verb_mode",1}, {"verb_sync",4}, {"verb_swell",0.6f}, {"verb_size",0.7f}, {"verb_damp",0.45f}, {"verb_mix",0.4f}, {"width",1.3f} } },
        { "Drum Room Suck", kUtil, { {"order",2}, {"verb_len",220}, {"verb_swell",0.8f}, {"verb_size",0.5f}, {"verb_mix",0.4f},
                                     {"fuzz_gain",0.4f}, {"fuzz_bias",0.5f}, {"fuzz_gate",0.5f}, {"fuzz_tone",0.6f}, {"fuzz_level",-8} } },
        { "Init", kUtil, {} },
    };
}
} // namespace

const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets = build();
    return presets;
}

const std::vector<std::string>& presetBanks()
{
    static const std::vector<std::string> banks = { kGlide, kWall, kReverse, kFuzz, kUtil };
    return banks;
}
} // namespace vf
