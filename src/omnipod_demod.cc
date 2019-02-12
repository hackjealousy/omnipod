#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <omnipod_demod.h>
#include <gr_io_signature.h>
#include <gr_complex.h>


omnipod_demod_sptr omnipod_make_demod(double clock_speed, unsigned int decimation) {

	return omnipod_demod_sptr(new omnipod_demod(clock_speed, decimation));
}


static const int MIN_IN = 1;	// mininum number of input streams
static const int MAX_IN = 1;	// maximum number of input streams
static const int MIN_OUT = 0;	// minimum number of output streams
static const int MAX_OUT = 0;	// maximum number of output streams


omnipod_demod::omnipod_demod(double clock_speed, unsigned int decimation) :
   gr_block ("omnipod_demod", gr_make_io_signature(MIN_IN, MAX_IN, sizeof(gr_complex)), gr_make_io_signature(MIN_OUT, MAX_OUT, sizeof(gr_complex))) {

	m_clock_speed = clock_speed;
	m_decimation = decimation;
	m_sr = clock_speed / decimation;

	m_sps = (unsigned int)(m_sr / m_symbol_rate);
	m_jitter = m_sps / 4;

	m_average_len = m_avg_n * m_sps;	// average over m_avg_n symbols
	m_average_a = 0;
	m_average_b = 0;

	m_sign = -1;
	m_count = 0;
	m_change_count = 0;

	memset(m_dbuf, 0, sizeof(m_dbuf));
	m_dbuf_count = 0;

	m_rep = REP_MANCHESTER;
	m_hex = 0;

	m_fp = 0;
	m_rfp = 0;

	m_show_power = 0;
	m_show_samples = 0;

	m_sample_number = 0;
	m_signal_start = 0;
	m_last_signal_start = 0;

	if(!(m_cb = new circular_buffer(m_cb_len, sizeof(gr_complex), 1))) {
		throw std::runtime_error("error: cannot create circular buffer");
	}
	if(!(m_signal_cb = new circular_buffer(m_cb_len, sizeof(gr_complex), 0))) {
		throw std::runtime_error("error: cannot create circular buffer for signal");
	}

	set_history(2 * m_average_len + 1 + 1);
}


omnipod_demod::~omnipod_demod() {

	if(m_fp)
		fclose(m_fp);

	if(m_rfp)
		fclose(m_rfp);

	if(m_cb)
		delete m_cb;

	if(m_signal_cb)
		delete m_signal_cb;
}


void omnipod_demod::set_representation(int rep) {

	m_rep = (rep_type)rep;
}


void omnipod_demod::show_hex() {

	m_hex = 1;
}


void omnipod_demod::set_output(char *filename) {

	if(!(m_fp = fopen(filename, "a"))) {
		throw std::runtime_error("error: set_output: cannot open file for writing");
	}
}


void omnipod_demod::set_capture(char *filename) {

	char buf[BUFSIZ];

	snprintf(buf, sizeof(buf), "%s-%.1fMHz-%u.omnidump", filename, m_clock_speed / 1e6, m_decimation);
	if(!(m_rfp = fopen(buf, "a"))) {
		throw std::runtime_error("error: set_raw_output: cannot open file for writing");
	}
}


void omnipod_demod::show_power() {

	m_show_power = 1;
}


void omnipod_demod::show_samples() {

	m_show_samples = 1;
}


void omnipod_demod::do_printf(const char *fmt, ...) {

	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	if(m_fp) {
		va_start(ap, fmt);
		vfprintf(m_fp, fmt, ap);
		va_end(ap);
	}
}


void omnipod_demod::display_hex(char *data, unsigned int data_len) {

	unsigned int i, h = 0, h_count = 0, first = 1;

	for(i = 0; i < data_len; i++) {
		h = (h << 1) | data[i];
		h_count += 1;
		if(h_count >= 32) {
			if(!first)
				do_printf(" ");
			else
				first = 0;
			do_printf("%8.8x", h);
			h = 0;
			h_count = 0;
		}
	}
	if(h_count) {
		h = h << (32 - h_count);
		if(!first)
			do_printf(" ");
		do_printf("%8.8x", h);
	}
}


void omnipod_demod::display_c_hex(char *data, unsigned int data_len) {

	unsigned i, h = 0, h_count = 0, first = 1;

	for(i = 0; i < data_len; i++) {
		if((data[i] == '0') || (data[i] == '1')) {
			h = (h << 1) | (data[i] - '0');
			h_count += 1;
			if(h_count >= 32) {
				if(!first)
					do_printf(" ");
				else
					first = 0;
				do_printf("%8.8x", h);
				h = 0;
				h_count = 0;
			}
		} else {
			if(h_count) {
				h = h << (32 - h_count);
				if(!first)
					do_printf(" ");
				else
					first = 0;
				do_printf("%8.8x", h);
				h = 0;
				h_count = 0;
			}
			if(!first)
				do_printf(" ");
			else
				first = 0;
			do_printf("%c", data[i]);
		}
	}
	if(h_count) {
		h = h << (32 - h_count);
		if(!first)
			do_printf(" ");
		do_printf("%8.8x", h);
	}
}


void omnipod_demod::display_c_hex_bytes_le(char *data, unsigned int data_len) {

	unsigned int i, h = 0, h_count = 0, b_count = 0;

	for(i = 0; i < data_len; i++) {
		if((data[i] == '0') || (data[i] == '1')) {
			h = h | ((data[i] - '0') << h_count);
			h_count += 1;
			if(h_count >= 8) {
				if((b_count > 0) && (b_count % 4 == 0))
					do_printf(" ");
				do_printf("%2.2x", h);
				b_count += 1;
				h = 0;
				h_count = 0;
			}
		} else {
			if(h_count > 0) {
				if((b_count > 0) && (b_count % 4 == 0))
					do_printf(" ");
				do_printf("%2.2x", h);
				b_count += 1;
				h = 0;
				h_count = 0;
			}
			if(b_count > 0)
				do_printf(" ");
			do_printf("%c", data[i]);
			b_count = 4;
		}
	}
	if(h_count > 0) {
		if((b_count > 0) && (b_count % 4 == 0))
			do_printf(" ");
		do_printf("%2.2x", h);
	}
}


void omnipod_demod::display_c_hex_bytes(char *data, unsigned int data_len) {

	unsigned int i, h = 0, h_count = 0, b_count = 0;

	for(i = 0; i < data_len; i++) {
		if((data[i] == '0') || (data[i] == '1')) {
			h = (h << 1) | (data[i] - '0');
			h_count += 1;
			if(h_count >= 8) {
				if((b_count > 0) && (b_count % 4 == 0))
					do_printf(" ");
				do_printf("%2.2x", h);
				b_count += 1;
				h = 0;
				h_count = 0;
			}
		} else {
			if(h_count > 0) {
				h = h << (8 - h_count);
				if((b_count > 0) && (b_count % 4 == 0))
					do_printf(" ");
				do_printf("%2.2x", h);
				b_count += 1;
				h = 0;
				h_count = 0;
			}
			if(b_count > 0)
				do_printf(" ");
			do_printf("%c", data[i]);
			b_count = 4;
		}
	}
	if(h_count > 0) {
		h = h << (8 - h_count);
		if((b_count > 0) && (b_count % 4 == 0))
			do_printf(" ");
		do_printf("%2.2x", h);
	}
}


void omnipod_demod::save_signal() {

	static int first_save = 1;

	unsigned int i, nitems;
	gr_complex zero = 0, one = 1, *buf;


	if(!m_rfp)
		return;

	if(first_save) {
		for(i = 0; i < 2 * m_average_len; i++)
			fwrite(&zero, sizeof(gr_complex), 1, m_rfp);
		first_save = 0;
	}

	buf = (gr_complex *)m_signal_cb->peek(&nitems);
	fwrite(buf, sizeof(gr_complex), nitems, m_rfp);

	// need to make sure that slice() is called before eof
	for(i = 0; i < 4 * m_average_len; i++)
		fwrite(&zero, sizeof(gr_complex), 1, m_rfp);
	for(i = 0; i < 4 * m_average_len; i++)
		fwrite(&one, sizeof(gr_complex), 1, m_rfp);
}


static int do_put(char *buf, unsigned int bufsize, unsigned int &o, const char *c) {

	unsigned int s = strlen(c);

	if(o + s < bufsize - 1) {
		memcpy(buf + o, c, s);
		o += s;
		return 0;
	}
	return 1;
}


static unsigned int manchester_decode(unsigned char *dbuf, unsigned int dbuf_count, char *data, unsigned int max_data_len) {

	unsigned int data_len, i;

	data_len = 0;
	for(i = 0; i < dbuf_count - 1;) {
		switch(dbuf[i]) {
			case 0: // 0
				switch(dbuf[i + 1]) {
					case 0:		// 0 0		error no phase change; perhaps missed first symbol
						do_put(data, max_data_len, data_len, "*");
						i += 1;
						break;
					case 1:		// 0 1
						do_put(data, max_data_len, data_len, "0");
						i += 2;
						break;
					case 2:		// 0 v		impossible error
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 3:		// 0 ^		error violation in center; perhaps missed first symbol
						do_put(data, max_data_len, data_len, "*");
						i += 1;
						break;
					case 4:		// 0 0 v	impossible error
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 5:		// 0 1 ^
						do_put(data, max_data_len, data_len, "0^");
						i += 2;
						break;
					case 6:		// 0 0 v 0	impossible error
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 7:		// 0 1 ^ 1
						do_put(data, max_data_len, data_len, "0^");
						dbuf[i + 1] = 1;
						i += 1;
						break;
					default:
						do_put(data, max_data_len, data_len, "X");
						i += 2;
				}
				break;

			case 1: // 1
				switch(dbuf[i + 1]) {
					case 0:		// 1 0
						do_put(data, max_data_len, data_len, "1");
						i += 2;
						break;
					case 1:		// 1 1		error no phase change; perhaps missed first symbol
						do_put(data, max_data_len, data_len, "*");
						i += 1;
						break;
					case 2:		// 1 v		error violation in center; perhaps missed first symbol
						do_put(data, max_data_len, data_len, "*");
						i += 1;
						break;
					case 3:		// 1 ^		impossible error
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 4:		// 1 0 v
						do_put(data, max_data_len, data_len, "1v");
						i += 2;
						break;
					case 5:		// 1 1 ^	impossible error
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 6:		// 1 0 v 0
						do_put(data, max_data_len, data_len, "1v");
						dbuf[i + 1] = 0;
						i += 1;
						break;
					case 7:		// 1 1 ^ 1	impossible error
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					default:
						do_put(data, max_data_len, data_len, "X");
						i += 2;
				}
				break;

			case 2: // v
				do_put(data, max_data_len, data_len, "v");
				i += 1;
				break;

			case 3: // ^
				do_put(data, max_data_len, data_len, "^");
				i += 1;
				break;

			case 4: // v 0	-- since first, assuming violation comes before symbol
				switch(dbuf[i + 1]) {
					case 0:		// v 0 0	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 1:		// v 0 1
						do_put(data, max_data_len, data_len, "v0");
						i += 2;
						break;
					case 2:		// v 0 v	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 3:		// v 0 ^	error violation in center
						do_put(data, max_data_len, data_len, "v*");
						i += 1;
						break;
					case 4:		// v 0 0 v	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 5:		// v 0 1 ^
						do_put(data, max_data_len, data_len, "v0^");
						i += 2;
						break;
					case 6:		// v 0 0 v 0	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 7:		// v 0 1 ^ 1
						do_put(data, max_data_len, data_len, "v0^");
						dbuf[i + 1] = 1;
						i += 1;
						break;
					default:
						do_put(data, max_data_len, data_len, "X");
						i += 2;
				}
				break;

			case 5: // ^ 1
				switch(dbuf[i + 1]) {
					case 0:		// ^ 1 0
						do_put(data, max_data_len, data_len, "^1");
						i += 2;
						break;
					case 1:		// ^ 1 1	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 2:		// ^ 1 v	error violation in center
						do_put(data, max_data_len, data_len, "^*");
						i += 1;
						break;
					case 3:		// ^ 1 ^	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 4:		// ^ 1 0 v
						do_put(data, max_data_len, data_len, "^1v");
						i += 2;
						break;
					case 5:		// ^ 1 1 ^	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 6:		// ^ 1 0 v 0
						do_put(data, max_data_len, data_len, "^1v");
						dbuf[i + 1] = 0;
						i += 1;
						break;
					case 7:		// ^ 1 1 ^ 1	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					default:
						do_put(data, max_data_len, data_len, "X");
						i += 2;
				}
				break;

			case 6: // 0 v 0
				switch(dbuf[i + 1]) {
					case 0:		// 0 v 0 0	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 1:		// 0 v 0 1	error violation in center
						do_put(data, max_data_len, data_len, "*v0");
						i += 2;
						break;
					case 2:		// 0 v 0 v	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 3:		// 0 v 0 ^	error violation in center
						do_put(data, max_data_len, data_len, "*");
						i += 1;
						break;
					case 4:		// 0 v 0 0 v	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 5:		// 0 v 0 1 v	error violation in center
						do_put(data, max_data_len, data_len, "*v0v");
						i += 2;
						break;
					case 6:		// 0 v 0 0 v 0	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 7:		// 0 v 0 1 ^ 1	error violation in center
						do_put(data, max_data_len, data_len, "*v0^");
						dbuf[i + 1] = 1;
						i += 1;
						break;
					default:
						do_put(data, max_data_len, data_len, "X");
						i += 2;
				}
				break;

			case 7: // 1 ^ 1
				switch(dbuf[i + 1]) {
					case 0:		// 1 ^ 1 0	error violation in center
						do_put(data, max_data_len, data_len, "*^1");
						i += 2;
						break;
					case 1:		// 1 ^ 1 1 	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 2:		// 1 ^ 1 v	error violation in center
						do_put(data, max_data_len, data_len, "*");
						i += 1;
						break;
					case 3:		// 1 ^ 1 ^	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 4:		// 1 ^ 1 0 v	error violation in center
						do_put(data, max_data_len, data_len, "*^1v");
						i += 2;
						break;
					case 5:		// 1 ^ 1 1 ^	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					case 6:		// 1 ^ 1 0 v 0	error violation in center
						do_put(data, max_data_len, data_len, "*^1v");
						dbuf[i + 1] = 0;
						i += 1;
						break;
					case 7:		// 1 ^ 1 1 ^ 1	impossible
						do_put(data, max_data_len, data_len, "#");
						i += 1;
						break;
					default:
						do_put(data, max_data_len, data_len, "X");
						i += 2;
				}
				break;

			default:
				do_put(data, max_data_len, data_len, "X");
				i += 1;
		}
	}
	data[data_len] = 0;

	return data_len;
}


void omnipod_demod::decode_compressed() {

	unsigned int i;

	if(m_show_samples)
		do_printf("sample: %9llu (%.1lfms)\t", m_signal_start, 1000.0 * (double)(m_signal_start - m_last_signal_start) / m_sr);
	if(m_show_power)
		do_printf("power: %.1f\t", m_power);
	for(i = 0; i < m_dbuf_count; i++)
		switch(m_dbuf[i]) {
			case 0:
				do_printf("_");
				break;
			case 1:
				do_printf("-");
				break;
			case 2:
				do_printf("v");
				break;
			case 3:
				do_printf("^");
				break;
			case 4:
				do_printf("_v");
				break;
			case 5:
				do_printf("-^");
				break;
			case 6:
				do_printf("_v_");
				break;
			case 7:
				do_printf("-^-");
				break;
			default:
				do_printf("*");
		}
	do_printf("\n");

	// can't tell if this is a "good" signal, so just save it
	save_signal();
}


void omnipod_demod::decode_nrz() {

	unsigned int i;

	if(m_show_samples)
		do_printf("sample: %9llu (%.1lfms)\t", m_signal_start, 1000.0 * (double)(m_signal_start - m_last_signal_start) / m_sr);
	if(m_show_power)
		do_printf("power: %.1f\t", m_power);
	for(i = 0; i < m_dbuf_count; i++)
		switch(m_dbuf[i]) {
			case 0:
				do_printf("0");
				break;
			case 1:
				do_printf("1");
				break;
			case 2:
				do_printf("v");
				break;
			case 3:
				do_printf("^");
				break;
			case 4:
				do_printf("0v");
				break;
			case 5:
				do_printf("1^");
				break;
			case 6:
				do_printf("0v0");
				break;
			case 7:
				do_printf("1^1");
				break;
			default:
				do_printf("*");
		}
	do_printf("\n");

	// can't tell if this is a "good" signal, so just save it
	save_signal();
}


void omnipod_demod::decode_manchester() {

	unsigned int i, data_len;
	char data[2 * BUFSIZ];

	data_len = manchester_decode(m_dbuf, m_dbuf_count, data, sizeof(data));
	if(data_len) {
		if(m_show_samples)
			// do_printf("sample: %9llu (%7u)\t", m_signal_start, m_signal_start - m_last_signal_start);
			do_printf("sample: %9llu (%.1lfms)\t", m_signal_start, 1000.0 * (double)(m_signal_start - m_last_signal_start) / m_sr);
		if(m_show_power)
			do_printf("power: %.1f:\t", m_power);
		if(m_hex) {
			// display_c_hex_bytes_le(data, data_len);
			display_c_hex_bytes(data, data_len);
			do_printf(":\t");
		}

		int dno = 0;
		for(i = 0; i < data_len; i++) {
			if((data[i] == '0') || (data[i] == '1')) {
				if((dno > 0) && (dno % 4 == 0))
					do_printf(" ");
				do_printf("%c", data[i]);
				dno += 1;
			} else {
				do_printf(" %c ", data[i]);
				dno = 0;
			}
		}
		do_printf("\n");

		// valid signal, save it
		save_signal();
	}
}


void omnipod_demod::decode_manchester_strict() {

	static unsigned char preamble[] = {1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0};
	static unsigned int preamble_len = sizeof(preamble) / sizeof(*preamble);

	unsigned int i, data_len;
	char data[2 * BUFSIZ];

	if(m_dbuf_count < preamble_len)
		return;
	for(i = 0; i < m_dbuf_count - preamble_len; i++)
		if(!memcmp(&m_dbuf[i], preamble, preamble_len))
			break;;
	if(i >= m_dbuf_count - preamble_len)
		return;

	/*
	 * We've identified the preamble except for the
	 * final half-bit and violation.  1.5-bit symbol.  If this symbol is
	 * followed by a high, we'll have detected a
	 * 2.5-bit symbol.  If followed by a low it will
	 * be a 1.5-bit symbol.
	 *
	 * XXX with normal preamble the half-bit symbols
	 * are high, but other times they can be low.
	 */
	i += preamble_len;

	if(m_dbuf[i] == 5) {
		i += 1;		// valid preamble end-symbol followed by low as first symbol of next bit
	} else if(m_dbuf[i] == 7) {
		m_dbuf[i] = 1;	// valid preamble end-symbol followed by high as first symbol of next bit
	} else {
		printf("preamble was %d\n", m_dbuf[i]);
		return;
	}

	data_len = 0;
	for(; i + 1 < m_dbuf_count; i += 2) {
		// if we have a half-bit symbol anywhere but the start, just finish
		if((m_dbuf[i] > 1) || (m_dbuf[i + 1] > 1))
			break;
		if((m_dbuf[i] == 0) && (m_dbuf[i + 1] == 1)) {
			data[data_len++] = 0;
		} else if((m_dbuf[i] == 1) && (m_dbuf[i + 1] == 0)) {
			data[data_len++] = 1;
		} else {
			do_printf("Manchester decoding error: symbol %u\n", i);
		}
	}

	if(data_len) {
		if(m_show_samples)
			do_printf("sample: %9llu (%.1lfms)\t", m_signal_start, 1000.0 * (double)(m_signal_start - m_last_signal_start) / m_sr);
		if(m_show_power)
			do_printf("power: %.1f:\t", m_power);
		if(m_hex) {
			display_hex(data, data_len);
			do_printf(":\t");
		}
		for(i = 0; i < data_len; i++)
			do_printf("%d", data[i]);
		do_printf("\n");

		// valid signal, save it
		save_signal();
	}
}


int bits_to_uchar(char *data, const unsigned int max_data_len, char *&p, unsigned int bits, unsigned char &c) {

	unsigned int i;

	c = 0;
	if(bits > 8)
		bits = 8;
	for(i = 0; ((p - data) < max_data_len) && (i < bits) && ((*p == '0') || (*p == '1')); i++)
		c = (c << 1) | (*p++ - '0');
	if(i >= bits)
		return 0;
	if(p - data >= max_data_len)
		return -1;
	p += (bits - i);
	return bits - i;
}


int bits_to_uint(char *data, const unsigned int max_data_len, char *&p, unsigned int bits, unsigned int &u) {

	unsigned int i;

	u = 0;
	if(bits > 32)
		bits = 32;
	for(i = 0; ((p - data) < max_data_len) && (i < bits) && ((*p == '0') || (*p == '1')); i++)
		u = (u << 1) | (*p++ - '0');
	if(i >= bits)
		return 0;
	if(p - data >= max_data_len)
		return -1;
	p += (bits - i);
	return bits - i;
}


void omnipod_demod::decode_protocol() {

	static const char *preamble = "1101111110^";
	static const unsigned int preamble_len = strlen(preamble);

	int r;
	unsigned int i, data_len, u;
	char data[2 * BUFSIZ], *p;

	data_len = manchester_decode(m_dbuf, m_dbuf_count, data, sizeof(data));
	if(!data_len)
		return;

	// valid signal, save it
	save_signal();

	if(!(p = strstr(data, preamble)))
		return;

	if(m_show_samples)
		do_printf("sample: %9llu (%.1lfms)\t", m_signal_start, 1000.0 * (double)(m_signal_start - m_last_signal_start) / m_sr);

	if(m_show_power)
		do_printf("power: %.1f:\t", m_power);

	// first find preamble
	do_printf("P:");
	p += preamble_len;

	// bit 0: expect more bursts
	if((r = bits_to_uint(data, data_len, p, 1, u))) {
		if(r < 0) {
			do_printf("\n");
			return;
		}
		do_printf(" X");
	} else
		do_printf(" %x", u);

	// bits 1 - 2: message type (?)
	if((r = bits_to_uint(data, data_len, p, 2, u))) {
		if(r < 0) {
			do_printf("\n");
			return;
		}
		do_printf(" X");
	} else
		do_printf(" %x", u);

	// bits 3 - 7: sequence number
	if((r = bits_to_uint(data, data_len, p, 5, u))) {
		if(r < 0) {
			do_printf("\n");
			return;
		}
		do_printf(" XX");
	} else
		do_printf(" %2.2x", u);

	// 4 unsigned int
	for(i = 0; i < 4; i++) {
		if((r = bits_to_uint(data, data_len, p, 32, u))) {
			if(r < 0) {
				do_printf("\n");
				return;
			}
			do_printf(" XXXXXXXX");
		} else
			do_printf(" %8.8x", u);
	}

	// unsigned short
	if((r = bits_to_uint(data, data_len, p, 16, u))) {
		if(r < 0) {
			do_printf("\n");
			return;
		}
		do_printf(" XXXX");
	} else
		do_printf(" %4.4x", u);

	// 4 4-bit
	for(i = 0; i < 4; i++) {
		if((r = bits_to_uint(data, data_len, p, 4, u))) {
			if(r < 0) {
				do_printf("\n");
				return;
			}
			do_printf(" X");
		} else
			do_printf(" %x", u);
	}

	// done
	do_printf(" !\n");
}


void omnipod_demod::represent() {

	unsigned int i, nitems;
	gr_complex *buf;

	// calculate average power of current signal
	if(m_show_power) {
		m_power = 0;
		buf = (gr_complex *)m_signal_cb->peek(&nitems);
		for(i = 0; i < nitems; i++)
			m_power += std::abs(buf[i]);
		m_power /= nitems;
	}

	switch(m_rep) {

		/*
		 * Note: in compressed and NRZ we assume that a
		 * violation symbol is always transmitted as following
		 * a valid symbol or between valid symbols.
		 */

		/*
		 * Display signal in "compressed" form.
		 */
		case REP_COMPRESSED:
			decode_compressed();
			break;

		/*
		 * Display the signal as NRZ.
		 */
		case REP_NRZ:
			decode_nrz();
			break;

		/*
		 * Manchester decode without regard for preamble.
		 */
		case REP_MANCHESTER:
			decode_manchester();
			break;

		/*
		 * Manchester decode the bits following the preamble.
		 */
		case REP_MANCHESTER_STRICT:
			decode_manchester_strict();
			break;

		case REP_DECODE:
			decode_protocol();
			break;

		default:
			do_printf("unknown representation\n");
	}
}


void omnipod_demod::slice() {

	unsigned int i, j;
	unsigned int nitems, max = 8 * m_average_len;
	double symbols = (double)m_count / (double)m_sps;
	gr_complex *buf;


	// we can detect at most m_avg_n - 1 sequential values
	for(i = 1; (i < m_avg_n - 1) && ((double)i - m_error < symbols); i++) {
		if(symbols <= ((double)i + m_error)) {
			// valid symbol

			// save valid samples to sample_cb
			buf = (gr_complex *)m_cb->peek(&nitems);
			if(m_count + m_jitter + 1 <= nitems) {
				buf += nitems - (m_count + m_jitter + 1);
				m_signal_cb->write(buf, m_count);
			}

			// if first valid symbol in burst, save start
			if(!m_dbuf_count) {
				m_last_signal_start = m_signal_start;
				m_signal_start = m_sample_number - (m_count + m_jitter + 1 + 2 * m_average_len);
			}

			for(j = 0; j < i; j++) {
				m_dbuf[m_dbuf_count++] = (m_sign >= 0);

				// if demodulated buffer is full, display it
				if(m_dbuf_count >= sizeof(m_dbuf)) {
					represent();
					m_signal_cb->flush();
					m_dbuf_count = 0;
				}
			}

			return;
		}
	}

	/*
	 * Half-symbol logic guesses:
	 *
	 * A half-symbol indicates a violation and usually separates the
	 * preamble and data.
	 *
	 * A half-symbol never occurs in the center of a bit.  (I.e.,
	 * between two symbols that represent a bit.)
	 *
	 * I'd like to assume that a violation always continues the last
	 * transmitted symbol, but I'm not positive.
	 *
	 * Only .5, 1.5, and 2.5 widths could possibly be transmitted
	 * normally for otherwise a bit was transmitted without a phase
	 * transition.
	 */

	// detect half-symbols
	for(i = 0; (i <= 2) && ((double)i + 0.5 - m_error < symbols); i++) {
		if(symbols <= ((double)i + 0.5 + m_error)) {
			// valid half-symbols

			// save valid samples to sample_cb
			buf = (gr_complex *)m_cb->peek(&nitems);
			if(m_count + m_jitter + 1 <= nitems) {
				buf += nitems - (m_count + m_jitter + 1);
				m_signal_cb->write(buf, m_count);
			}

			// if first valid symbol in burst, save start
			if(!m_dbuf_count) {
				m_last_signal_start = m_signal_start;
				m_signal_start = m_sample_number - (m_count + m_jitter + 1 + 2 * m_average_len);
			}

			m_dbuf[m_dbuf_count++] = (i + 1) * 2 + (m_sign >= 0);

			return;
		}
	}

	// this width did not match valid symbols
	if(m_dbuf_count > 0) {
		/*
		 * Since we have valid data and this is the first place
		 * we errored out, we want to preserve this data as
		 * well.  There could be a lot of junk data here so we
		 * limit the amount.
		 */
		buf = (gr_complex *)m_cb->peek(&nitems);
		if(m_count + m_jitter + 1 <= nitems) {
			buf += nitems - (m_count + m_jitter + 1);
			max = 8 * m_average_len;
			if(m_count + m_jitter < max)
				max = m_count + m_jitter;
			m_signal_cb->write(buf, max);
		}

		// display the buffer
		represent();
		m_signal_cb->flush();
		m_dbuf_count = 0;
	}

	return;
}


int omnipod_demod::general_work(int, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &) {

	static int starting_now = 1;

	const gr_complex *inc = (const gr_complex *)input_items[0];
	unsigned int nitems = (unsigned int)ninput_items[0], i, j;
	float cur;
	double avg;


	for(i = 0; i + 2 * m_average_len + 1 < nitems; i++) {

		// save input signal
		m_cb->write(&inc[i], 1);

		// 0 1 ... (len - 1) len (len + 1) ... (len + len - 1) 2len (2len + 1)
		//                          cur

		// pre-compute initial average
		if(starting_now) {
			m_average_a = 0;
			m_average_b = 0;
			for(j = 0; j < m_average_len; j++) {
				m_average_a += std::abs(inc[m_average_len + 1 + j]);
				m_average_b += std::abs(inc[j]);
			}
			m_sample_number = m_average_len;
			starting_now = 0;
		}

		m_sample_number += 1;

		// running averages
		cur = std::abs(inc[i + m_average_len + 1]);
		m_average_a = m_average_a - cur + std::abs(inc[i + 2 * m_average_len + 1]);
		m_average_b = m_average_b - std::abs(inc[i]) + std::abs(inc[i + m_average_len]);

		/*
		 * The start of the burst uses averages after the
		 * current sample.  The rest of the burst uses averages
		 * before the current sample.
		 */
		if(m_dbuf_count <= 2 * m_avg_n) {
			avg = m_average_a / m_average_len;
		} else {
			avg = m_average_b / m_average_len;
		}

		if(cur < avg) {
			if(m_sign < 0) {
				m_count += m_change_count + 1;
				m_change_count = 0;
			} else {
				// swapped from high to low
				if(m_change_count < m_jitter) {
					m_change_count += 1;
				} else {
					slice();
					m_sign = -1;
					m_count = m_change_count + 1;
					m_change_count = 0;
				}
			}
		} else {
			if(m_sign > 0) {
				m_count += m_change_count + 1;
				m_change_count = 0;
			} else {
				// swapped from low to high
				if(m_change_count < m_jitter) {
					m_change_count += 1;
				} else {
					slice();
					m_sign = 1;
					m_count = m_change_count + 1;
					m_change_count = 0;
				}
			}
		}
	}

	consume_each(i);
	return i;
}
