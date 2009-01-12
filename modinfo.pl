#!/usr/bin/perl
# Generate a fancy module list
# Copyright (C) 2007-2009 Daniel Collins <solemnwarning@solemnwarning.net>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#	* Redistributions of source code must retain the above copyright
#	  notice, this list of conditions and the following disclaimer.
#
#	* Redistributions in binary form must reproduce the above copyright
#	  notice, this list of conditions and the following disclaimer in the
#	  documentation and/or other materials provided with the distribution.
#
#	* Neither the name of the software author nor the names of any
#	  contributors may be used to endorse or promote products derived from
#	  this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE SOFTWARE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
# NO EVENT SHALL THE SOFTWARE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

my $modinfo = "/sbin/modinfo";
my $dirlist = "dirs.txt";

print <<EOF;
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>

<head>
<title>Kernel modules</title>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<style type="text/css">
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
	
	print "<li><a href=\"#dir_$dir\">$title ($dir/)</a></li>\n";
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
