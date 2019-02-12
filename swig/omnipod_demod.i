/* -*- c++ -*- */

%include "gnuradio.i"
%{
#include "omnipod_demod.h"
%}
%include "omnipod_demod.i"


GR_SWIG_BLOCK_MAGIC(omnipod, demod);

omnipod_demod_sptr omnipod_make_demod(double clock_speed = 64e6, unsigned int decimation = 256);

class omnipod_demod : public gr_block {

public:
        void set_representation(int rep);
        void set_output(char *filename);
        void set_capture(char *filename);
        void show_hex();
        void show_power();
        void show_samples();

private:
        omnipod_demod(double clock_speed, unsigned int decimation);
};

