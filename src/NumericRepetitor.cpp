#include "plugin.hpp"
#include "ProminateCommon.hpp"

struct NumericRepetitor : Module {
    enum ParamIds {
        PRIME_SELECT_PARAM,
        FACTOR1_PARAM,
        FACTOR2_PARAM,
        FACTOR3_PARAM,
        BANK_SELECT_PARAM,
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

    // "Prime" patterns from Noise Engineering Numeric Repetitor
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

    NumericRepetitor() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(PRIME_SELECT_PARAM, 0.f, 15.f, 0.f, "");
        configParam(FACTOR1_PARAM, 0.f, 15.f, 0.f, "");
        configParam(FACTOR2_PARAM, 0.f, 15.f, 0.f, "");
        configParam(FACTOR3_PARAM, 0.f, 15.f, 0.f, "");
        configParam(BANK_SELECT_PARAM, 0.f, 1.f, 0.f, "Bank.");
    }

    void setIndex(int index) {
        this->index = index;
        if (this->index >= NUM_STEPS) {
            this->index = 0;
        }
    }

    void process(const ProcessArgs &args) override {
        bool gateIn = false;

        float bank_select_param = params[BANK_SELECT_PARAM].getValue();
		int base_index = bank_select_param == 1.f ? 16 : 0;

		int prime_index = (int)clamp(
            std::round(params[PRIME_SELECT_PARAM].getValue()), 0.f, 15.f);
		if (inputs[PRIME_CV_INPUT].isConnected()) {
			if (inputs[PRIME_CV_INPUT].getVoltage() != 0.f) {
				prime_index = (int)(clamp(prime_index/10.0f * inputs[PRIME_CV_INPUT].getVoltage(), 0.0f, (float) prime_index));
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
            if (resetTrigger.process(inputs[RST_INPUT].getVoltage())) {
                setIndex(0);
            }
            gateIn = clockTrigger.isHigh();

            // PRIME
            int bit_status = (prime >> index) & 1;
            lights[PRIME_LIGHT].setBrightness((gateIn && bit_status) ? 1.f
                                                                     : 0.f);
            outputs[PRIME_OUTPUT].setVoltage((gateIn && bit_status) ? 10.f
                                                                    : 0.f);

            // PRODUCT 1
            int factor1_knob =
                (int)std::round(params[FACTOR1_PARAM].getValue());
            factor1_knob = clamp(factor1_knob, (16 - 1) * -1, 16 - 1);
            if (factor1_knob < 0) {
                factor1_knob *= -1;
            }
            uint16_t factor1 = factors[factor1_knob];

            uint32_t rhythm1 = prime * factor1;
            uint16_t final_rhythm1 =
                (uint16_t)((rhythm1 & 0xFFFF) | (rhythm1 >> 16));

            int bit_status1 = (final_rhythm1 >> index) & 1;
            lights[PRODUCT1_LIGHT].setBrightness((gateIn && bit_status1) ? 1.f
                                                                         : 0.f);
            outputs[PRODUCT1_OUTPUT].setVoltage((gateIn && bit_status1) ? 10.f
                                                                        : 0.f);

            // PRODUCT 2
            int factor2_knob =
                (int)std::round(params[FACTOR2_PARAM].getValue());
            factor2_knob = clamp(factor2_knob, (16 - 1) * -1, 16 - 1);
            if (factor2_knob < 0) {
                factor2_knob *= -1;
            }
            uint16_t factor2 = factors[factor2_knob];

            uint16_t mask2 = 0x0F0F;
            uint32_t rhythm2 = (prime & mask2) * factor2;
            uint16_t final_rhythm2 =
                (uint16_t)((rhythm2 & 0xFFFF) | (rhythm2 >> 16));

            int bit_status2 = (final_rhythm2 >> index) & 1;
            lights[PRODUCT2_LIGHT].setBrightness((gateIn && bit_status2) ? 1.f
                                                                         : 0.f);
            outputs[PRODUCT2_OUTPUT].setVoltage((gateIn && bit_status2) ? 10.f
                                                                        : 0.f);

            // PRODUCT 3
            int factor3_knob =
                (int)std::round(params[FACTOR3_PARAM].getValue());
            factor3_knob = clamp(factor3_knob, (16 - 1) * -1, 16 - 1);
            if (factor3_knob < 0) {
                factor3_knob *= -1;
            }
            uint16_t factor3 = factors[factor3_knob];

            uint16_t mask3 = 0xF003;
            uint32_t rhythm3 = (prime & mask3) * factor3;
            uint16_t final_rhythm3 =
                (uint16_t)((rhythm3 & 0xFFFF) | (rhythm3 >> 16));

            int bit_status3 = (final_rhythm3 >> index) & 1;
            lights[PRODUCT3_LIGHT].setBrightness((gateIn && bit_status3) ? 1.f
                                                                         : 0.f);
            outputs[PRODUCT3_OUTPUT].setVoltage((gateIn && bit_status3) ? 10.f
                                                                        : 0.f);
        }
    }
};

struct NumericRepetitorWidget : ModuleWidget {
    NumericRepetitorWidget(NumericRepetitor *module) {
        setModule(module);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance, "res/NumericRepetitor1.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(
            Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(
            Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(
            createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH,
                                          RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<SnapPromKnob>(
            mm2px(Vec(10.678, 19.749)), module,
            NumericRepetitor::PRIME_SELECT_PARAM));

        addParam(createParamCentered<SnapPromKnob>(
            Vec(29.2, 117), module, NumericRepetitor::FACTOR1_PARAM));

        addParam(createParamCentered<SnapPromKnob>(
            Vec(29.2, 171.9), module, NumericRepetitor::FACTOR2_PARAM));

        addParam(createParamCentered<SnapPromKnob>(
            Vec(29.2, 228.8), module, NumericRepetitor::FACTOR3_PARAM));

        addParam(createParamCentered<CKSS>(
            Vec(57.6, 328.2), module, NumericRepetitor::BANK_SELECT_PARAM));

        addInput(createInputCentered<PJ301MPort>(Vec(21.9, 289.8), module,
                                                 NumericRepetitor::RST_INPUT));
        addInput(createInputCentered<PJ301MPort>(
            Vec(21.9, 328.9), module, NumericRepetitor::EXT_CLOCK_INPUT));

		addInput(createInputCentered<PJ301MPort>(
            Vec(83.9, 65.2), module, NumericRepetitor::PRIME_CV_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(
            Vec(94.8, 211.5), module, NumericRepetitor::PRIME_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(94.8, 250.8), module, NumericRepetitor::PRODUCT1_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(94.8, 289.8), module, NumericRepetitor::PRODUCT2_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(94.8, 328.8), module, NumericRepetitor::PRODUCT3_OUTPUT));

        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(77.5, 220.4), module, NumericRepetitor::PRIME_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(77.5, 259.4), module, NumericRepetitor::PRODUCT1_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(77.5, 298.9), module, NumericRepetitor::PRODUCT2_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(77.5, 337.9), module, NumericRepetitor::PRODUCT3_LIGHT));

        addChild(createLightCentered<SmallLight<RedLight>>(
            Vec(60.4, 100.4), module, NumericRepetitor::BANK_SET_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(60.4, 112.6), module, NumericRepetitor::PATCH1_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(60.4, 124.8), module, NumericRepetitor::PATCH2_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(60.4, 137), module, NumericRepetitor::PATCH3_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(60.4, 149.2), module, NumericRepetitor::PATCH4_LIGHT));
    }
};

Model *modelNumericRepetitor =
    createModel<NumericRepetitor, NumericRepetitorWidget>("NumericRepetitor");
