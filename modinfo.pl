#!/usr/bin/perl
# Generate a fancy module list
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

my $modinfo = "/sbin/modinfo";
my $dirlist = "dirs.txt";

print <<EOF;
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>

<head>
<title>Kernel modules</title>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<style type="text/css">
body {
	color: #000000;
	background-color: #FFFFFF;
	font-family: sans-serif;
}

table {
	border-top: 1px solid #0000FF;
	border-bottom: 1px solid #0000FF;
	border-left: 1px solid #0000FF;
	border-collapse: collapse;
}

th {
	border-bottom: 1px solid #0000FF;
}

th, td {
	border-right: 1px solid #0000FF;
	padding: 2px 5px;
}

.link {
	color: #0000FF;
	text-decoration: none;
}

.link:hover {
	color: #FF0000;
	text-decoration: underline;
}
</style>
</head>

<body>
<h1>Kernel modules</h1>
<p>
To use a kernel module, copy it to the modules/ directory on the kexec-loader
boot floppy, you must also copy the dependencies (if any), the dependencies of
the dependencies, and so on...
</p>
<ul>
EOF

open(DIRLIST, "<$dirlist") or die("Error opening $dirlist: $!");

foreach my $dline(<DIRLIST>) {
	my ($dir, $title) = split(/\t+/, $dline, 2);
	
	print "<li><a class=\"link\" href=\"#dir_$dir\">$title ($dir/)</a></li>\n";
}

close(DIRLIST);

print "</ul>\n";

open(DIRLIST, "<$dirlist") or die("Error opening $dirlist: $!");

foreach my $dline(<DIRLIST>) {
	my ($dir, $title) = split(/\t+/, $dline, 2);
	opendir(DIR, $dir) or die("Error opening $dir: $!");
	
	print "<h2><a name=\"dir_$dir\">$title ($dir/)</a></h2>\n";
	print "<table>\n";
	print "<tr>\n";
	print "<th>Module</th><th>Description</th><th>Dependencies</th>\n";
	print "</tr>\n";
	
	foreach my $file(readdir(DIR)) {
		if($file =~ /.*\.ko/) {
			my @info = qx($modinfo $dir/$file);
			my $desc = "";
			my $depends = "";
			
			foreach my $line(@info) {
				chomp($line);
				my ($name, $value) = split(/:\s+/, $line, 2);
				
				if($name eq "description") {
					$desc = $value;
					
					$desc =~ s/&/&amp;/g;
					$desc =~ s/"/&quot;/g;
					$desc =~ s/</&lt;/g;
					$desc =~ s/>/&gt;/g;
				}
				if($name eq "depends") {
					foreach my $mod(split(/,/, $value)) {
						if($depends ne "") {
							$depends .= ", ";
						}
						
						$depends .= "<a class=\"link\" href=\"#mod_$mod\">$mod</a>";
					}
				}
			}
			
			my $name = $file;
			$name =~ s/\.ko//;
			
			if($desc eq "") {
				$desc = "<i>No description</i>";
			}
			if($depends eq "") {
				$depends = "<i>None</i>";
			}
			
			print "<tr>\n";
			print "<td><a name=\"mod_$name\">$name</a></td><td>$desc</td><td>$depends</td>\n";
			print "</tr>\n";
		}
	}
	
	print "</table>\n";
	
	closedir(DIR);
}

print "</body>\n";
print "</html>\n";

close(DIRLIST);
