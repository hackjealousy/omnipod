#!/usr/bin/env python
import sys
from gnuradio import gr, optfir, usrp
from gnuradio.eng_option import eng_option
from optparse import OptionParser
from omnipod import demod as omnidemod

omnipod_freq = 13.56e6


def pick_subdev(u):
	aid = u.db(0, 0).dbid()
	bid = u.db(1, 0).dbid()
	if (aid == 1) or (aid == 15):
		print "Using Side A: " + u.db(0, 0).name()
		return (0, 0)
	if (bid == 1) or (bid == 15):
		print "Using Side B: " + u.db(1, 0).name()
		return (1, 0)
	print "No suitable daughterboard found!"
	sys.exit(-1)


def demod(options):

	graph = gr.top_block();
	if options.input_file_name is not None:
		source = file_source = gr.file_source(gr.sizeof_gr_complex, options.input_file_name);
		if options.clock_speed is None:
			options.clock_speed = 64e6
	else:
		source = usrp.source_c(which = options.which, decim_rate = options.decimation);
		if options.clock_speed is not None:
			source.set_fpga_master_clock_freq(long(options.clock_speed))
		else:
			options.clock_speed = float(source.adc_rate())
		if options.rx_subdev_spec is None:
			options.rx_subdev_spec = pick_subdev(source);
		source.set_mux(usrp.determine_rx_mux_value(source, options.rx_subdev_spec));
		subdev = usrp.selected_subdev(source, options.rx_subdev_spec);
		g = subdev.gain_range();
		if options.gain is None:
			options.gain = float(g[0] + g[1]) / 2
		subdev.set_gain(options.gain * (g[1] - g[0]) + g[0]);
		if not source.tune(0, subdev, omnipod_freq):
			print "Failed to set frequency!";
			return

	# typedef enum {
	# 	REP_COMPRESSED,
	# 	REP_NRZ,
	# 	REP_MANCHESTER_STRICT,
	# 	REP_MANCHESTER,
	#	REP_DECODE
	# } rep_type;

	rep = options.representation.lower()
	if rep[0] == 'c':	# compressed
		repi = 0
	elif rep[0] == 'n':	# nrz
		repi = 1
	elif rep[0] == 's':	# manchester strict
		repi = 2
	elif rep[0] == 'm':	# manchester
		repi = 3
	elif rep[0] == 'd':	# decode
		repi = 4
	else:
		print "error: unknown representation"
		return

	demod_sink = omnidemod(options.clock_speed, options.decimation);
	demod_sink.set_representation(repi)
	if options.hex:
		demod_sink.show_hex()
        if options.show_power:
                demod_sink.show_power()
	if options.show_samples:
		demod_sink.show_samples()
	if options.output_file_name is not None:
		demod_sink.set_output(options.output_file_name)
	if options.capture_file is not None:
		demod_sink.set_capture(options.capture_file)

	graph.connect(source, demod_sink);
	graph.run()


def main():
	parser = OptionParser(option_class=eng_option)
	parser.add_option("-w", "--which", type = "int", default = 0,
	   help = "select which USRP (default is %default)")
	parser.add_option("-R", "--rx-subdev-spec", type = "subdev", default = None,
	   help = "select USRP RX side A or B")
	parser.add_option("-d", "--decimation", type = "int", default = 256,
	   help = "set FPGA decimation (default = %default)")
	parser.add_option("-F", "--clock-speed", type = "eng_float", default = None,
	   help = "set USRP clock speed (default = %default)")
	parser.add_option("-g", "--gain", type = "eng_float", default = None,
	   help = "set gain in dB [0.0, 1.0] (default is midpoint)")
	parser.add_option("-f", "--input-file-name", type = "string", default = None,
	   help = "set input to file (defaults to USRP)")
	parser.add_option("-o", "--output-file-name", type = "string", default = None,
	   help = "set output to file (defaults to screen)")
	parser.add_option("-r", "--representation", type = "string", default = "m",
	   help = "set representation: 'compressed', 'NRZ', 'Manchester', 'StrictManchester', 'Decode' (defaults to 'Manchester')")
        parser.add_option("-H", "--hex", action = "store_true", default = False,
           help = "include hex representation of data (default = %default)")
        parser.add_option("-p", "--show-power", action = "store_true", default = False,
           help = "show average power of each burst (default = %default)")
	parser.add_option("-s", "--show-samples", action = "store_true", default = False,
	   help = "show starting sample of captured burst (default = %default)")
	parser.add_option("-c", "--capture-file", type = "string", default = None,
	   help = "save captured signal bursts in ``filename-clock_speed-decimation.omnidump''")
	(options, args) = parser.parse_args()

	# do we still have arguments left over?
	if len(args) != 0:
		parser.print_help();
		sys.exit(1)

	demod(options)


if __name__ == '__main__':
	main()

