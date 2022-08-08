#pragma once

#include "tests.hpp"

class NAUTest: public BaseTest
{
public:
	NAUTest(const std::string &type = "NAU8810Test"): BaseTest(type){
		table_.insert({"read", {read, "- reads chip registers"}});
		table_.insert({"reset", {reset, "- soft chip reset"}});
		table_.insert({"sidetone", {sidetone, "- connect mic_in with audio_out"}});
		table_.insert({"micbias", {mic_bias, "[0 | 1] - enable mic power"}});
		table_.insert({"mout_att", {mout_attenuate, "[0 | 1] - "}});
		table_.insert({"mout_mute", {mout_mute, "[0 | 1 ] - "}});
		table_.insert({"mout_boost", {mout_boost, "[0 | 1] - "}});
		table_.insert({"mout_res", {mout_resistance, "[0 | 1] - "}});
		table_.insert({"pga_boost", {pga_boost, "[0 | 1] - "}});
		table_.insert({"pga_gain", {pga_gain, "<0..63> - "}});
		table_.insert({"dac_gain", {dac_gain, "<0..255> - "}});
		table_.insert({"dac_mout", {dac_mout, "[0 | 1] - "}});
		table_.insert({"dac_snr", {dac_best_SNR, "[0 | 1] - "}});
		table_.insert({"dac_soft_mute", {dac_soft_mute, "[0 | 1] - "}});
		table_.insert({"dac_automute", {dac_automute, "[0 | 1] - "}});
		table_.insert({"dac_deemp", {dac_deemp, "<32 | 44 | 48> - "}});
		table_.insert({"dac_lim", {dac_lim, "[0 | 1] - "}});
		table_.insert({"dac_lim_thr", {dac_lim_thr, "<-1..-6> - "}});
		table_.insert({"dac_lim_boost", {dac_lim_boost, "<0..12> - "}});
	}

	void init() const override;

private:
	static void read(const std::vector<std::string> &params);
	static void reset(const std::vector<std::string> &params);
	static void sidetone(const std::vector<std::string> &params);
	static void mic_bias(const std::vector<std::string> &params);
	static void mout_attenuate(const std::vector<std::string> &params);
	static void mout_mute(const std::vector<std::string> &params);
	static void mout_boost(const std::vector<std::string> &params);
	static void mout_resistance(const std::vector<std::string> &params);
	static void pga_boost(const std::vector<std::string> &params);
	static void pga_gain(const std::vector<std::string> &params);
	static void dac_gain(const std::vector<std::string> &params);
	static void dac_mout(const std::vector<std::string> &params);
	static void dac_best_SNR(const std::vector<std::string> &params);
	static void dac_soft_mute(const std::vector<std::string> &params);
	static void dac_automute(const std::vector<std::string> &params);
	static void dac_deemp(const std::vector<std::string> &params);
	static void dac_lim(const std::vector<std::string> &params);
	static void dac_lim_thr(const std::vector<std::string> &params);
	static void dac_lim_boost(const std::vector<std::string> &params);

	static uint16_t read_register(uint16_t reg_addr, const char *reg_name);
};
