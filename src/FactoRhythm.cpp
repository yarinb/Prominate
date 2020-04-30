#include "ProminateCommon.hpp"
#include "plugin.hpp"

struct FactoRhythm : Module {
  enum ParamIds {
    PRIME_SELECT_PARAM,
    FACTOR1_PARAM,
    FACTOR2_PARAM,
    FACTOR3_PARAM,
    BANK_SELECT_PARAM,
    RST_BUTTON,
    NUM_PARAMS
  };
  enum InputIds {
    RST_INPUT,
    EXT_CLOCK_INPUT,
    PRIME_CV_INPUT,
    FACTOR1_CV_INPUT,
    FACTOR2_CV_INPUT,
    FACTOR3_CV_INPUT,
    NUM_INPUTS
  };
  enum OutputIds {
    PRIME_OUTPUT,
    PRODUCT1_OUTPUT,
    PRODUCT2_OUTPUT,
    PRODUCT3_OUTPUT,
    NUM_OUTPUTS
  };
  enum LightIds {
    BANK_SET_LIGHT,
    PATCH1_LIGHT,
    PATCH2_LIGHT,
    PATCH3_LIGHT,
    PATCH4_LIGHT,
    PRIME_LIGHT,
    PRODUCT1_LIGHT,
    PRODUCT2_LIGHT,
    PRODUCT3_LIGHT,
    NUM_LIGHTS
  };

  int NUM_STEPS = 16;
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger resetTrigger;
  int index = 0;

  // "Prime" patterns
  // see manual https://www.noiseengineering.us/shop/numeric-repetitor
  // 1000100010001000
  // 1000100010001010
  // 1000100010010010
  // 1000100010010100
  // 1000100010100010
  // 1000100010100100
  // 1000100100010010
  // 1000100100010100
  // 1000100100100010
  // 1000100100100100
  // 1000101010001010
  // 1000101010101010
  // 1001001010010010
  // 1001001010101010
  // 1001010010101010
  // 1001010100101010
  // 1000001010000010
  // 1000001010001010
  // 1000001010010010
  // 1000001010100010
  // 1000010010000100
  // 1000010010001010
  // 1000010010010010
  // 1000010010010100
  // 1000010010100010
  // 1000010010100100
  // 1000010100001010
  // 1000010100010010
  // 1000010100010100
  // 1000010100100010
  // 1000010100100100
  // 1000010101000100
  const int table_nr[32] = {
      0x8888, 0x888A, 0x8892, 0x8894, 0x88A2, 0x88A4, 0x8912, 0x8914,
      0x8922, 0x8924, 0x8A8A, 0x8AAA, 0x9292, 0x92AA, 0x94AA, 0x952A,
      0x8282, 0x828A, 0x8292, 0x82A2, 0x8484, 0x848A, 0x8492, 0x8494,
      0x84A2, 0x84A4, 0x850A, 0x8512, 0x8514, 0x8522, 0x8524, 0x8544};

  const int factors[16] = {1,  2, 4,  8,  16, 3,  6,  9,
                           12, 5, 10, 15, 7,  14, 11, 13};

  FactoRhythm() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam(RST_BUTTON, 0.f, 1.f, 0.f, "Reset Rhythm");
    configParam(PRIME_SELECT_PARAM, 0.f, 15.f, 0.f, "Main Rhythm");
    configParam(FACTOR1_PARAM, 0.f, 15.f, 0.f, "Seq 2");
    configParam(FACTOR2_PARAM, 0.f, 15.f, 0.f, "Seq 3");
    configParam(FACTOR3_PARAM, 0.f, 15.f, 0.f, "Seq 4");
    configParam(BANK_SELECT_PARAM, 0.f, 1.f, 0.f, "Bank Select");
  }

  void setIndex(int index) {
    this->index = index;
    if (this->index >= NUM_STEPS) {
      this->index = 0;
    }
  }

  void reset() {
    setIndex(0);
    outputs[PRIME_OUTPUT].setVoltage(0.f);
    outputs[PRODUCT1_OUTPUT].setVoltage(0.f);
    outputs[PRODUCT2_OUTPUT].setVoltage(0.f);
    outputs[PRODUCT3_OUTPUT].setVoltage(0.f);
  }

  void process(const ProcessArgs &args) override {
    bool gate_in = false;

    float bank_select_param = params[BANK_SELECT_PARAM].getValue();
    int base_index = bank_select_param == 1.f ? 16 : 0;

    int prime_index = (int)clamp(
        std::round(params[PRIME_SELECT_PARAM].getValue()), 0.f, 15.f);
    if (inputs[PRIME_CV_INPUT].isConnected()) {
      if (inputs[PRIME_CV_INPUT].getVoltage() != 0.f) {
        prime_index = (int)(clamp(prime_index / 10.0f *
                                      inputs[PRIME_CV_INPUT].getVoltage(),
                                  0.0f, (float)prime_index));
      }
    }

    int rhythm_number = base_index + prime_index;
    int prime = table_nr[rhythm_number];

    // patch lights
    lights[BANK_SET_LIGHT].setBrightness(bank_select_param);

    // patch light bit 1
    lights[PATCH1_LIGHT].setBrightness(prime_index & 1);
    // patch light bit 2
    lights[PATCH2_LIGHT].setBrightness((prime_index >> 1) & 1);
    // patch light bit 3
    lights[PATCH3_LIGHT].setBrightness((prime_index >> 2) & 1);
    // patch light bit 4
    lights[PATCH4_LIGHT].setBrightness((prime_index >> 3) & 1);

    if (inputs[EXT_CLOCK_INPUT].isConnected()) {
      // External clock
      if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].getVoltage())) {
        setIndex(index + 1);
      }
      // Reset
      bool rst_input_voltage =
          resetTrigger.process(inputs[RST_INPUT].getVoltage()) > 0.f;
      bool rst_button_pressed = params[RST_BUTTON].getValue() == 1.f;
      if (rst_input_voltage || rst_button_pressed) {
        reset();
        return;
      }
      gate_in = clockTrigger.isHigh();

      // PRIME
      int bit_status = (prime >> index) & 1;
      lights[PRIME_LIGHT].setBrightness((gate_in && bit_status) ? 1.f : 0.f);
      outputs[PRIME_OUTPUT].setVoltage((gate_in && bit_status) ? 10.f : 0.f);

      // PRODUCT 1
      int factor1_knob = (int)std::round(params[FACTOR1_PARAM].getValue());
      factor1_knob = clamp(factor1_knob, (16 - 1) * -1, 16 - 1);
      if (factor1_knob < 0) {
        factor1_knob *= -1;
      }
      if (inputs[FACTOR1_CV_INPUT].isConnected()) {
        if (inputs[FACTOR1_CV_INPUT].getVoltage() != 0.f) {
          factor1_knob = (int)(clamp(factor1_knob / 10.0f *
                                         inputs[FACTOR1_CV_INPUT].getVoltage(),
                                     0.0f, (float)factor1_knob));
        }
      }
      uint16_t factor1 = factors[factor1_knob];

      uint32_t rhythm1 = prime * factor1;
      uint16_t final_rhythm1 = (uint16_t)((rhythm1 & 0xFFFF) | (rhythm1 >> 16));

      int bit_status1 = (final_rhythm1 >> index) & 1;
      lights[PRODUCT1_LIGHT].setBrightness((gate_in && bit_status1) ? 1.f
                                                                    : 0.f);
      outputs[PRODUCT1_OUTPUT].setVoltage((gate_in && bit_status1) ? 10.f
                                                                   : 0.f);
      // PRODUCT 2
      int factor2_knob = (int)std::round(params[FACTOR2_PARAM].getValue());
      factor2_knob = clamp(factor2_knob, (16 - 1) * -1, 16 - 1);
      if (factor2_knob < 0) {
        factor2_knob *= -1;
      }
      if (inputs[FACTOR2_CV_INPUT].isConnected()) {
        if (inputs[FACTOR2_CV_INPUT].getVoltage() != 0.f) {
          factor2_knob = (int)(clamp(factor2_knob / 10.0f *
                                         inputs[FACTOR2_CV_INPUT].getVoltage(),
                                     0.0f, (float)factor2_knob));
        }
      }
      uint16_t factor2 = factors[factor2_knob];

      uint16_t mask2 = 0x0F0F;
      uint32_t rhythm2 = (prime & mask2) * factor2;
      uint16_t final_rhythm2 = (uint16_t)((rhythm2 & 0xFFFF) | (rhythm2 >> 16));

      int bit_status2 = (final_rhythm2 >> index) & 1;
      lights[PRODUCT2_LIGHT].setBrightness((gate_in && bit_status2) ? 1.f
                                                                    : 0.f);
      outputs[PRODUCT2_OUTPUT].setVoltage((gate_in && bit_status2) ? 10.f
                                                                   : 0.f);

      // PRODUCT 3
      int factor3_knob = (int)std::round(params[FACTOR3_PARAM].getValue());
      factor3_knob = clamp(factor3_knob, (16 - 1) * -1, 16 - 1);
      if (factor3_knob < 0) {
        factor3_knob *= -1;
      }
      if (inputs[FACTOR3_CV_INPUT].isConnected()) {
        if (inputs[FACTOR3_CV_INPUT].getVoltage() != 0.f) {
          factor3_knob = (int)(clamp(factor3_knob / 10.0f *
                                         inputs[FACTOR3_CV_INPUT].getVoltage(),
                                     0.0f, (float)factor3_knob));
        }
      }
      uint16_t factor3 = factors[factor3_knob];

      uint16_t mask3 = 0xF003;
      uint32_t rhythm3 = (prime & mask3) * factor3;
      uint16_t final_rhythm3 = (uint16_t)((rhythm3 & 0xFFFF) | (rhythm3 >> 16));

      int bit_status3 = (final_rhythm3 >> index) & 1;
      lights[PRODUCT3_LIGHT].setBrightness((gate_in && bit_status3) ? 1.f
                                                                    : 0.f);
      outputs[PRODUCT3_OUTPUT].setVoltage((gate_in && bit_status3) ? 10.f
                                                                   : 0.f);
    }
  }
};

struct FactoRhythmWidget : ModuleWidget {
  FactoRhythmWidget(FactoRhythm *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/FactoRhythm.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(
        createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(
        Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(
        box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addParam(createParamCentered<SnapPromKnob>(
        Vec(31.6, 55.4), module, FactoRhythm::PRIME_SELECT_PARAM));

    addParam(createParamCentered<SnapPromKnob>(Vec(30.6, 123.4), module,
                                               FactoRhythm::FACTOR1_PARAM));

    addParam(createParamCentered<SnapPromKnob>(Vec(30.6, 165.2), module,
                                               FactoRhythm::FACTOR2_PARAM));

    addParam(createParamCentered<SnapPromKnob>(Vec(31.6, 207), module,
                                               FactoRhythm::FACTOR3_PARAM));

    addParam(createParamCentered<CKSS>(Vec(19.6, 88.7), module,
                                       FactoRhythm::BANK_SELECT_PARAM));

    addParam(createParamCentered<CKD6>(Vec(60, 343.3), module,
                                       FactoRhythm::RST_BUTTON));
    addInput(createInputCentered<PJ301MPort>(Vec(90.9, 343.3), module,
                                             FactoRhythm::RST_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(29.3, 342.2), module,
                                             FactoRhythm::EXT_CLOCK_INPUT));

    addInput(createInputCentered<PJ301MPort>(Vec(85.7, 55.4), module,
                                             FactoRhythm::PRIME_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(85.7, 123.4), module,
                                             FactoRhythm::FACTOR1_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(85.7, 165.2), module,
                                             FactoRhythm::FACTOR2_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(85.7, 207), module,
                                             FactoRhythm::FACTOR3_CV_INPUT));

    addOutput(createOutputCentered<PJ301MPort>(Vec(29.3, 245), module,
                                               FactoRhythm::PRIME_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(91, 245), module,
                                               FactoRhythm::PRODUCT1_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(29.3, 293.3), module,
                                               FactoRhythm::PRODUCT2_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(90.9, 293.3), module,
                                               FactoRhythm::PRODUCT3_OUTPUT));

    addChild(createLightCentered<SmallLight<GreenLight>>(
        Vec(43.9, 235.9), module, FactoRhythm::PRIME_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(
        Vec(105.6, 235.9), module, FactoRhythm::PRODUCT1_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(
        Vec(43.9, 284.3), module, FactoRhythm::PRODUCT2_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(
        Vec(105.5, 284.3), module, FactoRhythm::PRODUCT3_LIGHT));

    addChild(createLightCentered<SmallLight<RedLight>>(
        Vec(34.5, 87.6), module, FactoRhythm::BANK_SET_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(
        Vec(47.3, 87.6), module, FactoRhythm::PATCH1_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(
        Vec(60, 87.6), module, FactoRhythm::PATCH2_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(
        Vec(72.7, 87.6), module, FactoRhythm::PATCH3_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(
        Vec(85.5, 87.6), module, FactoRhythm::PATCH4_LIGHT));
  }
};

Model *modelFactoRhythm =
    createModel<FactoRhythm, FactoRhythmWidget>("FactoRhythm");
