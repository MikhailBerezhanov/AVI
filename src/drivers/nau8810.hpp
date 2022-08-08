#ifndef _NAU8810_HPP
#define _NAU8810_HPP

#include <cstdint>

#define CHIP_ADDR    0x34

// ----- NAU8810 Registers -----
// Power managment
#define SOFTWARE_RESET		( (uint8_t)(0x00) )

#define POWER_MANAGMENT_1	( (uint8_t)(0x01) )
#define DCBUFEN				( (uint16_t)(1 << 8) )	// DC level shifting enable (required for 1.5x gain)
#define PLLEN 				( (uint16_t)(1 << 5) )	// PLL enable (1 - En)
#define MICBIASEN			( (uint16_t)(1 << 4) )	// Microphone bias enable (1 - En)
#define ABIASEN				( (uint16_t)(1 << 3) )	// Analogue amplifier bias control (1 - En)
#define IOBUFEN				( (uint16_t)(1 << 2) )	// Unused i\o tie off buffer enable (1 - En)
#define REFIMP1				( (uint16_t)(1 << 1) )	// VREF impedance selection
#define REFIMP0				( (uint16_t)(1 << 0) )	// 00 - dis, 01 - 80k, 10 - 300k, 11 - 3k

#define POWER_MANAGMENT_2	( (uint8_t)(0x02) )
#define BSTEN               ( (uint16_t)(1 << 4) )  // Input Boost enable (1 - En)
#define PGAEN               ( (uint16_t)(1 << 2) )  // MIC(+/-) PGA Enable (1 - En)
#define ADCEN               ( (uint16_t)(1 << 0) )  // ADC Enable (1 - En)

#define POWER_MANAGMENT_3	( (uint8_t)(0x03) )
#define MOUTEN              ( (uint16_t)(1 << 7) )  // MOUT Enable (1 - En)
#define NSPKEN              ( (uint16_t)(1 << 6) )  // SPKOUT- Enable (1 - En)
#define PSPKEN              ( (uint16_t)(1 << 5) )  // SPKOUT+ Enable(1 - En)
#define BIASGEN             ( (uint16_t)(1 << 4) )  // Bias Enable (1 - En)
#define MOUTMXEN            ( (uint16_t)(1 << 3) )  // MONO Mixer Enable (1 - En)
#define SPKMXEN             ( (uint16_t)(1 << 2) )  // Speaker Mixer Enable (1 - En)
#define DACEN               ( (uint16_t)(1 << 0) )  // DAC Enable (1 - En)

// Audio control
#define AUDIO_INTERFACE		( (uint8_t)(0x04) )
#define COMPANDING			( (uint8_t)(0x05) )
#define CLOCK_CONTROL_1		( (uint8_t)(0x06) )
#define CLOCK_CONTROL_2		( (uint8_t)(0x07) )

#define DAC_CTRL			( (uint8_t)(0x0A) )
#define DACMT              	( (uint16_t)(1 << 6) )  // Soft mute (1 - En)
#define DEEMP_1             ( (uint16_t)(1 << 5) )  // De-emphasis (11 - 48kHz)
#define DEEMP_0             ( (uint16_t)(1 << 4) )  // 
#define DACOS           	( (uint16_t)(1 << 3) )  // Over Sample Rate (0 -64x, 1 - 128x
#define AUTOMT             	( (uint16_t)(1 << 2) )  // Auto mute Enable (1 - En)
#define DACPL               ( (uint16_t)(1 << 0) )  // Polarity Invert (1 - En)


#define DAC_VOLUME			( (uint8_t)(0x0B) )	
#define ADC_CTRL			( (uint8_t)(0x0E) )
#define ADC_VOLUME			( (uint8_t)(0x0F) )

// Equalizer

// DAC limiter
#define DAC_LIMITER_1		( (uint8_t)(0x18) )
#define DACLIMEN			( (uint16_t)(1 << 8) )	// 1 - En DAC Digital Limiter
#define DACLIMDCY_7_4		// how fast the digital peak limiter increase the gain 
#define DACLIMATK_3_0		// how fast the digital peak limiter decrease the gain

#define DAC_LIMITER_2		( (uint8_t)(0x19) )
#define DACLIMTHL_2			( (uint16_t)(1 << 6) )
#define DACLIMTHL_1			( (uint16_t)(1 << 5) )
#define DACLIMTHL_0			( (uint16_t)(1 << 4) )
#define DACLIMBST_3_0

// Notch filter

// ALC control
#define ALC_CTRL_1			( (uint8_t)(0x20) )
#define ALC_CTRL_2			( (uint8_t)(0x21) )
#define ALC_CTRL_3			( (uint8_t)(0x22) )
#define NOISE_GATE			( (uint8_t)(0x23) )

// PLL control

// Input, Output and Mixer control
#define ATTENUATION_CTRL	( (uint8_t)(0x2B) )
#define MOUTATT             ( (uint16_t)(1 << 2) )  // Attenuation control for bypass path MONO mixer
#define SPKATT              ( (uint16_t)(1 << 1) )  // Speaker mixer 

#define INPUT_CTRL			( (uint8_t)(0x2C) )
#define MICBIASV8           ( (uint16_t)(1 << 8) )  // Microphone Bias Voltage Control
#define MICBIASV7           ( (uint16_t)(1 << 7) )  // 00 0.9*VDDA, 11 0.5*VDDA
#define NMICPGA             ( (uint16_t)(1 << 1) )  // MICN to input PGA negative terminal
#define PMICPGA             ( (uint16_t)(1 << 0) )  // Input PGA amplifier positive terminal to MIC+ or VREF

#define PGA_GAIN			( (uint8_t)(0x2D) )
#define PGAZC               ( (uint16_t)(1 << 7) )  // PGA Zero Cross Enable
#define PGAMT               ( (uint16_t)(1 << 6) )  // Mute Control for PGA
#define PGAGAIN_5_0         

#define ADC_BOOST			( (uint8_t)(0x2F) )
#define PGABST              ( (uint16_t)(1 << 8) )  // Input Boost (0 - +0 dB, 1 - +20 dB - default)
#define PMICBSTGAIN6        ( (uint16_t)(1 << 6) )  // MIC+ pin to the input Boost Stage
#define PMICBSTGAIN5        ( (uint16_t)(1 << 5) )  // NOTE: when using this path set PMICPGA = 0 
#define PMICBSTGAIN4        ( (uint16_t)(1 << 4) )  // 000 - disconnected, 001 -12 dB, 111 +6 dB

#define OUTPUT_CTRL			( (uint8_t)(0x31) )
#define MOUTBST				( (uint16_t)(1 << 3) )	// MONO output gain ( 0 - x1.0 or 1 - x1.5)
#define SPKBST				( (uint16_t)(1 << 2) )	// Speaker output gain (0 - x1.0 or 1 - x1.5)
#define TSEN				( (uint16_t)(1 << 1) )	// Thermal Shutdown (0 - Dis)
#define AOUTIMP				( (uint16_t)(1 << 0) )	// Analog output resistance (0 -1k or 1-30k)

#define MIXER_CTRL			( (uint8_t)(0x32) )
#define BYPSPK				( (uint16_t)(1 << 1) )	// Bypass path to Speaker (0 - disconnected)
#define DACSPK				( (uint16_t)(1 << 0) )	// DAC to speaker Mixer (1 - connected)

#define SPKOUT_VOLUME		( (uint8_t)(0x36) )
#define SPKZC				( (uint16_t)(1 << 7) )	// Speaker gain control
#define SPKMT				( (uint16_t)(1 << 6) )	// Speaker output (0 - En, 1 - muted)

#define MONO_MIXER_CONTROL	( (uint8_t)(0x38) )
#define MOUTMXMT			( (uint16_t)(1 << 6) )	// MOUT Mute (1 - Muted)
#define BYPMOUT				( (uint16_t)(1 << 1) )	// Bypass path to MONO Mixer (1 - Connected)
#define DACMOUT				( (uint16_t)(1 << 0) )	// DAC to MONO Mixer (1 - Connected)

// Low power control
#define POWER_MANAGMENT_4   ( (uint8_t)(0x3A) )

// PCM time slot, ADCOUT impedance option control

// Register ID

// -----------------------------

namespace hw{

class NAU8810
{
public:
	NAU8810(const std::string &i2c_dev = "/dev/i2c-5") { init(i2c_dev); }

	void init(const std::string &i2c_dev = "/dev/i2c-5");

	uint16_t read_reg(uint16_t reg);

	void soft_reset();

	void PGA_mute(bool on_off = true);
	void PGA_boost(bool on_off = true);
	void PGA_set_gain(uint8_t gain);

	void DAC_set_volume(uint8_t gain);
	void DAC_to_MOUT(bool on_off = true);
	void DAC_soft_mute(bool on_off);
	void DAC_automute(bool on_off);
	void DAC_de_emphasis(uint8_t rate);
	void DAC_best_SNR(bool on_off);
	void DAC_enable_limiter(bool on_off);
	void DAC_set_limiter_threshold(int8_t thr);
	void DAC_set_limiter_boost(uint16_t val);

	void MIC_bias(bool on_off = true);
	void MIC_sidetone();

	void MOUT_attenuate(bool on_off = true);
	void MOUT_mute(bool on_off = true);
	void MOUT_boost(bool on_off = true);
	void MOUT_add_resistance(bool on_off = true);

private:
	void write_reg(uint8_t reg, uint16_t value);
	void edit_bit(uint8_t reg, uint16_t bitmask, bool set);
};

} // namespace hw

#endif
