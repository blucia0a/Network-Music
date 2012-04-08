#!/usr/bin/perl
local $| = 1;
use warnings;
use strict;

my $num_clients = 0;
my %clientMap;
my %toneMap;

if ($#ARGV != 0) {
  die("Usage: Sift.pl [config file]");
}

# Read in the config file.
my $filename = shift @ARGV;
open FILE, "<" . $filename or die $!;
my @config = <FILE>;
close(FILE);

my @possibleNotes = split(/\s+/, $config[0]);

while(<>){
  
  chomp; 
  my ($srcip,$dstip,$srcport,$dstport,$icmp,$icmpsize) = split /\s+/; 


#  if( defined $icmp ){
#
#   
#    if( defined $icmpsize ){
#      warn "ICMP Size was $icmpsize\n";
#    }
#
#    &addClientIfNew($srcip,$dstip);
#    &doCommunication($srcip,1,'send');
#    &doCommunication($dstip,1,'recv');
#  }
  
  next if !defined $srcip;
  next if !defined $dstip;
  next if !defined $srcport;
  next if !defined $dstport;


  &addClientIfNew($srcip,$dstip);
  if( $srcport >= 41000 && $srcport < 41008 ){
    
    &doInstrumentCommunication($srcport - 41000, $srcport,'send');

  }else{

    &doCommunication($srcip,$srcport,'send');

  }

  if( $dstport >= 41000 && $dstport < 41008 ){
    
    &doInstrumentCommunication($dstport - 41000, $dstport,'recv');

  }else{
  
    &doCommunication($dstip,$dstport,'recv');

  }


}


sub doInstrumentCommunication(){

  my $instrument = shift;
  my $port = shift;
  my $direction = shift;
  my $dir = 0;
  if($direction eq 'send'){ $dir = 0; }else{ $dir = 1; }

  my $velocity = 117;

  my $tone = 0; 
  if( !exists $toneMap{$instrument} ){
    $toneMap{$instrument} = $possibleNotes[int(rand(14))]; 
  }
  $tone = $toneMap{$instrument} + ($dir * 12);

  my $duration = int(rand(1000000)) + 500000;

  #API is "instrument velocity tone duration"
  print "".$instrument." $velocity $tone $duration\n";
  
}

sub doCommunication(){

  my $ip = shift;
  my $port = shift;
  my $direction = shift;
  my $dir = 0;
  if($direction eq 'send'){ $dir = 0; }else{ $dir = 1; }

  my $velocity = int((1.0 - ($port / 65536))*127);
  my $tone = $toneMap{$clientMap{$ip}} + ($dir * 12);
  my $duration = int(rand(1000000)) + 500000;

  #API is "instrument velocity tone duration"
  print "".$clientMap{$ip}." $velocity $tone $duration\n";
  
}

sub addClientIfNew(){

  my $srcip = shift;
  my $dstip = shift;

  if( !exists $clientMap{$srcip} ){
    $clientMap{$srcip} = $num_clients++;
    $toneMap{ $clientMap{$srcip} } = $possibleNotes[int(rand(14))]; 
    #print "New client: $srcip\n"
  } 

  if( !exists $clientMap{$dstip} ){
    $clientMap{$dstip} = $num_clients++;
    $toneMap{$clientMap{$dstip}} = $possibleNotes[int(rand(14))]; 
    #print "New client: $dstip\n"
  } 

}
