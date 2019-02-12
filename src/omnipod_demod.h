#ifndef INCLUDED_OMNIPOD_DEMOD_H
#define INCLUDED_OMNIPOD_DEMOD_H

#include <stdio.h>
#include <stdarg.h>
#include <gr_block.h>
#include "circular_buffer.h"

typedef enum {
	REP_COMPRESSED,
	REP_NRZ,
	REP_MANCHESTER_STRICT,
	REP_MANCHESTER,
	REP_DECODE
} rep_type;


class omnipod_demod;

typedef boost::shared_ptr<omnipod_demod> omnipod_demod_sptr;

omnipod_demod_sptr omnipod_make_demod(double clock_speed = 64e6, unsigned int decimation = 256);

class omnipod_demod : public gr_block {
public:
	~omnipod_demod();
	int general_work(int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
	void set_representation(int rep);
	void set_output(char *filename);
	void set_capture(char *filename);
	void show_hex();
	void show_power();
	void show_samples();

private:
	double		m_clock_speed;
	unsigned int	m_decimation;

	double		m_sr;				// sample rate
	unsigned int	m_sps;				// samples per symbol
	unsigned int	m_jitter;			// amplitude must hold for at least this many samples to count

	unsigned int	m_average_len;
	double		m_average_a;			// average of samples after current
	double		m_average_b;			// average of samples before current

	int		m_sign;				// last sample was over / under average
	unsigned int	m_count;			// count of over / under
	unsigned int	m_change_count;			// don't change sign unless passed jitter threshold

	unsigned char	m_dbuf[BUFSIZ];			// buffer for demodulated signal
	unsigned int	m_dbuf_count;			// number of valid symbols in dbuf

	rep_type	m_rep;				// representation type
	int		m_hex;				// display in hex

	FILE *		m_fp;				// output file stream
	FILE *		m_rfp;				// raw output file stream

	circular_buffer *m_cb;				// circular buffer to save raw input
	circular_buffer *m_signal_cb;			// circular buffer to save valid signal

	int		m_show_power;			// display average power when burst displayed
	int		m_show_samples;			// display starting sample of each burst

	unsigned long long m_sample_number;		// current sample number;
	unsigned long long m_signal_start;		// current signal starting number
	unsigned long long m_last_signal_start;		// last signal starting number

	double		m_power;			// power in current signal

	static const double	  m_symbol_rate = 4000;	// from documentation (assuming Manchester, bit rate is half this)
	static const unsigned int m_avg_n = 8;		// average over 8 symbols
	static const unsigned int m_cb_len = (1 << 20);	// circular buffer length

	static const double m_error = 0.25;		// max error in width of symbol (XXX 0.25 is very wide...)

	friend omnipod_demod_sptr omnipod_make_demod(double, unsigned int);
	omnipod_demod(double clock_speed, unsigned int decimation);
	void slice();
	void represent();
	void save_signal();
	void do_printf(const char *fmt, ...);
	void display_hex(char *data, unsigned int data_len);
	void display_c_hex(char *data, unsigned int data_len);
	void display_c_hex_bytes(char *data, unsigned int data_len);
	void display_c_hex_bytes_le(char *data, unsigned int data_len);

	void decode_compressed();
	void decode_nrz();
	void decode_manchester();
	void decode_manchester_strict();
	void decode_protocol();
};
#endif /* INCLUDED_OMNIPOD_DEMOD_H */
