#!/usr/bin/perl
# Create TAR packages of kernel modules
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

my %groups = (
	"ide-pcmcia" => ["ide-cs"],
	"ide" => ["drivers/ide"],
	"memstick" => ["drivers/memstick/"],
	"mmc-sd" => ["drivers/mmc/"],
	"paride" => ["drivers/block/paride"],
	"firewire" => ["drivers/ieee1394"],
	"sata" => ["drivers/ata", "sx8"],
	"pcmcia" => ["drivers/pcmcia"],
	"parport" => ["drivers/parport"]
);

my @ignore = (
	"jbd",
	"jbd2",
	"firmware_class",
	"zlib_inflate",
	"cdrom"
);

if(@ARGV != 1) {
	print STDERR "Usage: tarmods.pl <output directory>\n";
	exit(1);
}

my $outdir = $ARGV[0];
my %mods = ();
my @dirs = ();

sub mod_deps {
	my ($module) = @_;
	my @mi_out = qx(/sbin/modinfo $module);
	my @depends = ();
	
	foreach my $line(@mi_out) {
		chomp($line);
		my ($name, $value) = split(/:\s+/, $line, 2);
		
		if($name eq "depends") {
			push(@depends, split(/,/, $value));
		}
	}
	
	return @depends;
}

sub exec_cmd {
	system($_[0]);
	
	if($? == -1 || ($? >> 8) != 0) {
		print STDERR "Failed to execute \'$_[0]\'\n";
		exit(1);
	}
}

sub copy_mod {
	my ($module, $dir) = @_;
	
	if(!defined($mods{$module})) {
		print("Warning: Unknown module \'$module\'\n");
		return;
	}
	
	my $file = $mods{$module}->[1];
	my @deps = mod_deps($file);
	
	exec_cmd("cp \"$file\" \"$dir\"");
	
	foreach my $mod(@deps) {
		copy_mod($mod, $dir);
	}
}

exec_cmd("rm -rf $outdir/*");

foreach my $file(qx(find -iname '*.ko')) {
	chomp($file);
	
	my $dir = $file;
	$dir =~ s/\.\///g;
	$dir =~ s/\/[^\/]*$//;
	
	my $mod = $file;
	$mod =~ s/.*\///g;
	$mod =~ s/\.ko//;
	
	$mods{$mod} = [$dir, $file];
}

foreach my $mod(keys(%mods)) {
	if(grep(/^$mod$/, @ignore)) {
		next;
	}
	
	my $group = $mod;
	
	my $dir = $mods{$mod}->[0];
	my $file = $mods{$mod}->[1];
	
	foreach my $key(keys(%groups)) {
		my $val = $groups{$key};
		
		if(grep(/^$dir$/, @$val) || grep(/^$mod$/, @$val)) {
			$group = $key;
			last;
		}
		
		foreach my $foo(@$val) {
			if($foo =~ /\/$/ && $dir =~ /^$foo/) {
				$group = $key;
				goto DONE;
			}
		}
	}
	
	DONE:
	
	if(!grep(/^$group$/, @dirs)) {
		push(@dirs, $group);
	}
	
	exec_cmd("mkdir -p \"$outdir/$group/\"");
	copy_mod($mod, "$outdir/$group");
}

foreach my $dir(@dirs) {
	my $fdir = "$outdir/$dir";
	
	exec_cmd("tar --format=v7 -C \"$fdir\" -cf \"$fdir.tar\" ./");
	exec_cmd("rm -rf \"$fdir\"");
	exec_cmd("lzma -9 \"$fdir.tar\"");
	exec_cmd("mv \"$fdir.tar.lzma\" \"$fdir.tlz\"");
}

foreach my $mod(keys(%mods)) {
	if(-e "$outdir/$mod.tlz" && !defined($groups{$mod})) {
		my $dir = $mods{$mod}->[0];
		
		if($dir eq "fs/nls") {
			$dir = "nls";
		}elsif($dir =~ /^fs\//) {
			$dir = "fs";
		}elsif($dir =~ /scsi/) {
			$dir =~ s/^drivers\///;
		}else{
			next;
		}
		
		exec_cmd("mkdir -p \"$outdir/$dir/\"");
		exec_cmd("mv \"$outdir/$mod.tlz\" \"$outdir/$dir/\"");
	}
}
