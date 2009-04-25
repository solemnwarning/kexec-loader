#!/usr/bin/perl
# Convert a console-tools keymap to a kexec-loader keymap
# Copyright (C) 2007-2009 Daniel Collins <solemnwarning@solemnwarning.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

use strict;
use warnings;

my $infile = undef;
my $outfile = undef;
my $path = "/usr/share/keymaps/include:/usr/share/keymaps/i386/include";
my $pw = 0;

foreach my $arg(@ARGV) {
	if($pw) {
		$path = $arg;
		$pw = 0;
		
		next;
	}
	
	if(substr($arg, 0, 1) eq "-") {
		if($arg eq "-h") {
			print "Usage: ./conv-keymap.pl [-p include-path] <input keymap> <output keymap>\n";
			exit(0);
		}elsif($arg eq "-p") {
			$pw = 1;
		}else{
			print STDERR "Usage: ./conv-keymap.pl [-p include-path] <input keymap> <output keymap>\n";
			exit(1);
		}
	}elsif(!defined($infile)) {
		$infile = $arg;
	}elsif(!defined($outfile)) {
		$outfile = $arg;
	}else{
		print STDERR "Usage: ./conv-keymap.pl [-p include-path] <input keymap> <output keymap>\n";
		exit(1);
	}
}

if(!defined($outfile)) {
	print STDERR "Usage: ./conv-keymap.pl [-p include-path] <input keymap> <output keymap>\n";
	exit(1);
}

my $t = $infile;
$t =~ s/\/[^\/]+$//;
$path .= ":$t";

open(TMP, ">$outfile") or die("Can't open keymap.txt: $!");

sub parse_keymap {
	my $file = $_[0];
	my @data = ();
	
	if(-e $file) {
		goto READFILE;
	}
	if(-e "$file.gz") {
		$file = "$file.gz";
		goto READFILE;
	}
	
	foreach my $dir(split(/:/, $path)) {
		if(-e "$dir/$file") {
			$file = "$dir/$file";
			goto READFILE;
		}
		if(-e "$dir/$file.gz") {
			$file = "$dir/$file.gz";
			goto READFILE;
		}
		if(-e "$dir/$file.inc") {
			$file = "$dir/$file.inc";
			goto READFILE;
		}
		if(-e "$dir/$file.inc.gz") {
			$file = "$dir/$file.inc.gz";
			goto READFILE;
		}
	}
	
	READFILE:
	if($file =~ /\.gz$/) {
		@data = qx(zcat $file);
	}else{
		@data = qx(cat $file);
	}
	
	foreach my $line(@data) {
		chomp($line);
		
		$line =~ s/^\s+//g;
		$line =~ s/\s+$//g;
		$line =~ s/\s*#.*//g;
		
		if($line eq "") {
			next;
		}
		
		if($line =~ /keycode\s+\d+\s*=/) {
			my ($a, $b) = split(/\s*=\s*/, $line, 2);
			my @as = split(/\s+/, $a);
			my $key = pop(@as);
			my $cmd = join(" ", @as);
			my @actions = split(/\s+/, $b);
			
			my $m = 0;
			
			foreach my $c(@as) {
				$m |= 1 if($c eq "shift");
				$m |= 2 if($c eq "altgr");
				$m |= 4 if($c eq "control");
				$m |= 8 if($c eq "alt");
				$m |= 16 if($c eq "shiftl");
				$m |= 32 if($c eq "shiftr");
				$m |= 64 if($c eq "ctrll");
				$m |= 128 if($c eq "ctrlr");
			}
			
			print TMP "$m\t$key\t$actions[0]\n" if(@actions > 0);
			print TMP "1\t$key\t$actions[1]\n" if(@actions > 1);
			print TMP "2\t$key\t$actions[2]\n" if(@actions > 2);
			print TMP "4\t$key\t$actions[3]\n" if(@actions > 3);
		}
		
		if($line =~ /include ".+"/) {
			my (undef, $ifile) = split(/"/, $line);
			parse_keymap($ifile);
		}
	}
}

parse_keymap($infile);

close(TMP);
