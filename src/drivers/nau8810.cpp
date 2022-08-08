// I2C Audio-codec NAU8810 simple driver for:
//      * MIC to MOUT sidetone bypass control
//      * PGA gain control (input microphone volume)
//      * DAC gain control (output audio volume)
//      * MICBIAS power control

#include "cstring"

#include "i2c.hpp"
#include "nau8810.hpp"

namespace hw{

void NAU8810::init(const std::string &i2c_dev)
{
    i2c_init(i2c_dev);
}

uint16_t NAU8810::read_reg(uint16_t reg)
{
    // The 7-MSB bits “0011010” are the device address

    uint8_t buf[2]; // NAU response with 2 data bytes 
    memset(buf, 0, sizeof buf);

    reg <<= 1;      // NAU protocol requeres shifted reg-addr

    i2c_read(CHIP_ADDR, reg, buf, sizeof buf);

    // log_dbg("(0x%02X) '%s': 0x%02X%02X\n", reg >> 1, reg_name, buf[0], buf[1]);

    uint16_t res = buf[1];
    res |= buf[0] << 8;

    return res;
}

void NAU8810::write_reg(uint8_t reg, uint16_t value)
{
    // first 7 bit is reg addr, rest 9 - is data
    uint16_t nau_reg = 0;
    uint8_t data = 0;

    nau_reg = (value & 0x0100);         // get 9th data bit
    nau_reg |= (uint16_t)(reg << 9);    // add reg address   
    nau_reg >>= 8;                      // make reg size as 1 byte

    data = (uint8_t)(value & 0xFF);

    // log_msg(MSG_VERBOSE, "nau write addr: 0x%02X, data: 0x%02X\n", nau_reg, data);

    i2c_write(CHIP_ADDR, nau_reg, &data, 1);
}

void NAU8810::edit_bit(uint8_t reg, uint16_t bitmask, bool set)
{
    uint16_t tmp = read_reg(reg);
    uint16_t val = set ? (tmp | bitmask) : (tmp & (~bitmask));  // set or unset bit

    write_reg(reg, val);
}

void NAU8810::soft_reset()
{
    // Performing a write instruction to this register with any data
    // will reset all the bits in the register map to default
    write_reg(SOFTWARE_RESET, 0x0000);
}


// Управление громкостью микрофона
void NAU8810::PGA_set_gain(uint8_t gain)
{
    // Only 6 bits available
    if(gain > 0b00111111) gain = 63;

    write_reg(PGA_GAIN, gain); 
}

// Отключение микрофона от PGA
void NAU8810::PGA_mute(bool on_off)
{
    edit_bit(PGA_GAIN, PGAMT, on_off);
}

// ADC input gain. true (+ 20 dB gain),  false (0 dB gain)
void NAU8810::PGA_boost(bool on_off)
{
    edit_bit(ADC_BOOST, PGABST, on_off);
}


// Управление громкостью аудио
void NAU8810::DAC_set_volume(uint8_t gain)
{
    // 0 - Digital mute
    // 0xFF - 0.0 dB (MAX)
    write_reg(DAC_VOLUME, gain);
}

// True - enable automute
void NAU8810::DAC_automute(bool on_off)
{
     edit_bit(DAC_CTRL, AUTOMT, on_off);
}

// True - enable soft mute
void NAU8810::DAC_soft_mute(bool on_off)
{
     edit_bit(DAC_CTRL, DACMT, on_off);
}

// The digital de-emphasis can be enabled by setting DEEMP[5:4] bits depending 
// on the input sample rate. It includes on-chip digital de-emphasis and is 
// available for sample rates of 32 kHz, 44.1 kHz, and 48 kHz.
void NAU8810::DAC_de_emphasis(uint8_t rate)
{
    switch(rate){
        case 32:
            edit_bit(DAC_CTRL, DEEMP_0, true);
            break;

        case 44:
            edit_bit(DAC_CTRL, DEEMP_1, true);
            break;

        case 48:
            edit_bit(DAC_CTRL, DEEMP_1 | DEEMP_0, true);
            break;

        default:
            // No de-emphasis
            edit_bit(DAC_CTRL, DEEMP_1 | DEEMP_0, false);
    }
}


// -6 .. -1 dB
void NAU8810::DAC_set_limiter_threshold(int8_t thr)
{
    uint16_t tmp = read_reg(DAC_LIMITER_2);
    tmp &= 0x018F;   // reset current threshold

    switch(thr){

        case -1:
            write_reg(DAC_LIMITER_2, tmp);
            break;

        case -2:
            tmp |= DACLIMTHL_0;
            write_reg(DAC_LIMITER_2, tmp);
            break;

        case -3:
            tmp |= DACLIMTHL_1;
            write_reg(DAC_LIMITER_2, tmp);
            break;

        case -4:
            tmp |= DACLIMTHL_1 | DACLIMTHL_0;
            write_reg(DAC_LIMITER_2, tmp);
            break;

        case -5:
            tmp |= DACLIMTHL_2;
            write_reg(DAC_LIMITER_2, tmp);
            break;

        case -6:
            tmp |= DACLIMTHL_2 | DACLIMTHL_0;
            write_reg(DAC_LIMITER_2, tmp);
            break;
    }
}

void NAU8810::DAC_set_limiter_boost(uint16_t val)
{
    // Only 4 bits available and 0..12 values
    if(val > 12) val = 12;

    uint16_t tmp = read_reg(DAC_LIMITER_2);
    tmp &= 0x01F0; // reset current boost value 
    uint16_t new_val = tmp | val; 

    write_reg(DAC_LIMITER_2, new_val);
}


void NAU8810::DAC_enable_limiter(bool on_off)
{
    write_reg(DAC_LIMITER_1, 0x0000); // fastest Attack and Decay
    edit_bit(DAC_LIMITER_1, DACLIMEN, on_off);
}

// True - enable 128x (best SNR), Flas - 64x (Lowest power)
void NAU8810::DAC_best_SNR(bool on_off)
{
    edit_bit(DAC_CTRL, DACOS, on_off);
}

// DAC to MONO Mixer. False - Disconnected, True - Connected
void NAU8810::DAC_to_MOUT(bool on_off)
{
    edit_bit(MONO_MIXER_CONTROL, DACMOUT, on_off);
}

// During mute, the MONO output will output VREF that can be  used as a DC 
// reference for a headphone out. True - Muted, False - Not Muted
void NAU8810::MOUT_mute(bool on_off)
{
    edit_bit(MONO_MIXER_CONTROL, MOUTMXMT, on_off);
}

// true - 1.5x MOUT gain, false - 1.0x MOUT gain
void NAU8810::MOUT_boost(bool on_off)
{   
    edit_bit(OUTPUT_CTRL, MOUTBST, on_off);
}

// Analog Output Resistance
// true - 30k, false - 1k
void NAU8810::MOUT_add_resistance(bool on_off)
{   
    edit_bit(OUTPUT_CTRL, AOUTIMP, on_off);
}


// Attenuation Control for bypass path (output of input boost stage) to MONO mixer input
void NAU8810::MOUT_attenuate(bool on_off)
{   
    edit_bit(ATTENUATION_CTRL, MOUTATT, on_off);
}

// Подать питание MICBIAS на микрофон, но не подключать сигнал к выходу MOUT 
// True - MIC power on, False - MIC power off
void NAU8810::MIC_bias(bool on_off)
{
    if(on_off){
        // Set MICBIAS Voltage to 0.5 VDDA, and enable NMICPGA, PMICPGA
        write_reg(INPUT_CTRL, MICBIASV8 | MICBIASV7 | NMICPGA | PMICPGA);
    }
    
     edit_bit(POWER_MANAGMENT_1, DCBUFEN | MICBIASEN | REFIMP0 | ABIASEN, on_off);
}

void NAU8810::MIC_sidetone()
{
    // Enable BYPMOUT[1] and DACMOUT[0] in Mono Mixer Comtrol
    write_reg(MONO_MIXER_CONTROL, BYPMOUT | DACMOUT);

    // Set MICBIAS Voltage to 0.5 VDDA, and enable NMICPGA, PMICPGA
    write_reg(INPUT_CTRL, MICBIASV8 | MICBIASV7 | NMICPGA | PMICPGA);

    // Disable MOUTBST[3] (1.0X Gain), SPKBST[2], TSEN[1], AOUTIMP[0] - 30k in Output ctrl
    write_reg(OUTPUT_CTRL, /*MOUTBST |*/ SPKBST | TSEN | AOUTIMP);

    // Enable SPKMT[6] in SPKOUT Volume (Speaker Muted)
    write_reg(SPKOUT_VOLUME, SPKMT);

    // Отключаем усиление +20 дБ PGA по умолчанию
    PGA_boost(false);

    // Power 
    // DCBUFEN[8], REFIMP[1:0] = 01, IOBUFEN, ABIASEN, MICBIASEN, 
    // write_reg(POWER_MANAGMENT_1, 0x015D);
    write_reg(POWER_MANAGMENT_1, DCBUFEN | REFIMP0 | ABIASEN | MICBIASEN);  

    // BSTEN, PGAEN, ADCEN
    write_reg(POWER_MANAGMENT_2, 0x0015); 

    // DACEN, MOUTMXEN, MOUTEN
    write_reg(POWER_MANAGMENT_3, 0x0089);
}     

} // namespace hw
