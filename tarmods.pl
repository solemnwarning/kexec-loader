#!/usr/bin/perl
# Create a modules package from a kernel tree
# Copyright (C) 2007-2010 Daniel Collins <solemnwarning@solemnwarning.net>
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

if(@ARGV != 1) {
	print STDERR "Usage: tarmods.pl <output directory>\n";
	exit 1;
}

my $output = $ARGV[0];

my @modules_list = (
	# Random crap that isn't directly used by anyone, so don't package
	#
	{ expr => "parport.ko" },
	{ expr => "zlib_inflate.ko" },
	{ expr => "firmware_class.ko" },
	{ expr => "jbd.ko" },
	{ expr => "jbd2.ko" },
	
	{ expr => "fs/nls/", dest => "nls/\$modname" },
	{ expr => "fs/", dest => "fs/\$modname" },
	
	{ expr => "drivers/ata/", dest => "sata" },
	{ expr => "sx8.ko", dest => "sata" },
	
	{ expr => "drivers/ide/", dest => "ide" },
	{ expr => "drivers/scsi/", dest => "scsi/\$modname" },
	{ expr => "drivers/ieee1394/", dest => "firewire" },
	{ expr => "drivers/memstick/", dest => "memstick" },
	{ expr => "drivers/mmc/", dest => "mmc" },
	{ expr => "drivers/block/paride/", dest => "paride" },
	{ expr => "drivers/pcmcia/", dest => "pcmcia" }
);

my %output_dirs = ();

my @modules = `find -iname '*.ko'` or die;

sub get_deps {
	my ($module) = @_;
	
	my $out = `modinfo -F depends "$module"`;
	chomp($out);
	
	return split(/,/, $out);
}

sub copy_module {
	my ($modpath, $dest) = @_;
	
	my $modfile = $modpath;
	$modfile =~ s/.*\///g;
	
	my $modname = $modfile;
	$modname =~ s/\.ko$//;
	
	$dest =~ s/\$modname/$modname/g;
	$output_dirs{$dest} = 1;
	
	system("mkdir -p \"$dest\"") == 0 or die;
	
	if(!-e "$dest/$modfile") {
		system("cp \"$modpath\" \"$dest\"") == 0 or die;
	}
	
	my @deps = get_deps($modpath);
	
	foreach my $dep(@deps) {
		my $found = 0;
		
		foreach my $mod(@modules) {
			chomp($mod);
			
			if($mod =~ /\/$dep\.ko$/) {
				copy_module($mod, $dest);
				
				$found = 1;
				last;
			}
		}
		
		die("Cannot find module \'$dep\'") if(!$found);
	}
}

foreach my $module(@modules) {
	chomp($module);
	
	my $match = 0;
	
	foreach my $m(@modules_list) {
		if($module =~ /$m->{"expr"}/) {
			if($m->{"dest"}) {
				my @dests = split(/,/, $m->{"dest"});
				
				foreach my $dest(@dests) {
					copy_module($module, "$output/$dest");
				}
			}
			
			$match = 1;
			last;
		}
	}
	
	if(!$match) {
		my $modname = $module;
		$modname =~ s/.*\///g;
		$modname =~ s/\.ko$//g;
		
		print("Unknown module: \'$module\'; packaging seperately\n");
		copy_module($module, "$output/$modname");
	}
}

foreach my $path(keys(%output_dirs)) {
	$path =~ s/\/$//;
	
	system("tar -cf $path.tar -C $path ./") == 0 or die;
	system("rm -r $path") == 0 or die;
	
	system("lzma -9c $path.tar > $path.tlz") == 0 or die;
	system("rm $path.tar") == 0 or die;
}
