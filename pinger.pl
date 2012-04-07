#!/usr/bin/perl

use Net::Ping;
use Time::HiRes;

my $host = shift @ARGV;
my $pause = shift @ARGV | 1000000;
my $p = Net::Ping->new();

while( 1 ){
  $p->ping( $host );
  Time::HiRes::usleep( $pause );
}
