#!/usr/bin/perl
local $| = 1;
use warnings;
use strict;

my $num_clients = 0;
my %clientMap;
my %Map;

while(<>){
  
  chomp; 
  my ($srcip,$dstip,$srcport,$dstport,$icmp) = split /\s+/; 


  if( defined $icmp ){
    &addClientIfNew($srcip,$dstip);
    &doCommunication($srcip,1,'send');
    &doCommunication($dstip,1,'recv');
  }
  
  next if !defined $srcip;
  next if !defined $dstip;
  next if !defined $srcport;
  next if !defined $dstport;


  &addClientIfNew($srcip,$dstip);
  &doCommunication($srcip,$srcport,'send');
  &doCommunication($dstip,$dstport,'recv');

}

sub doCommunication(){

  my $ip = shift;
  my $port = shift;
  my $direction = shift;

  if( $direction eq 'send' ){
    print "".$clientMap{$ip}." ".$port." 0\n";   
  }
  if( $direction eq 'recv' ){
    print "".$clientMap{$ip}." ".$port." 1\n";   
  }
  
}

sub addClientIfNew(){

  my $srcip = shift;
  my $dstip = shift;

  if( !exists $clientMap{$srcip} ){
    $clientMap{$srcip} = $num_clients++;
    #print "New client: $srcip\n"
  } 

  if( !exists $clientMap{$dstip} ){
    $clientMap{$dstip} = $num_clients++;
    #print "New client: $dstip\n"
  } 

}
