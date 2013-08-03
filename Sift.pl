#!/usr/bin/perl
local $| = 1;
use warnings;
use strict;

my $num_clients = 0;
my %clientMap;

my %noteSets;
my %minDurations;
my %maxDurations;

if ($#ARGV != 0) {
  die("Usage: Sift.pl [config file]");
}

# Read in the config file.
my $filename = shift @ARGV;
open FILE, "<" . $filename or die $!;
while(<FILE>){

  chomp;
  next if /#/;
  next if $_ eq "";
  if( /default/i ){

    my @notes = split /\s+/;
    shift @notes;
    $minDurations{'default'} = shift @notes;
    $maxDurations{'default'} = shift @notes;
    push @{$noteSets{'default'}}, @notes;

  }elsif( /instrument/i ){

    my @notes = split /\s+/;
    my $iNum = shift @notes;
    $iNum =~ s/instrument//i;
    $minDurations{$iNum} = shift @notes;
    $maxDurations{$iNum} = shift @notes;
    push @{$noteSets{$iNum}}, @notes;

  }else{

    warn "Weird line in config file: \"$_\".  Ignoring it.\n";

  }
  
}
close(FILE);

#my @possibleNotes = split /\s+/, $config[0];

while(<>){
  if ($_ !~ /^\d+\./) {
    next;
  }
  
  chomp; 
  my ($srcip,$dstip,$srcport,$dstport,$icmp,$icmpsize) = split /\s+/; 
  
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


  my $tone = 33;
  my $durationKey;
  if( exists $noteSets{ $instrument } ){
    $tone = $noteSets{ $instrument }->[int( rand( @{$noteSets{ $instrument }} ) )];
    $durationKey = $instrument;
  } else {
    $tone = $noteSets{ 'default' }->[int( rand( @{$noteSets{ 'default' }} ) )];
    $durationKey = 'default';
  }

  # Bound the duration between min/max.
  my $duration = &getDuration($durationKey);

  print "".$instrument." $velocity $duration $tone\n";
  
}

sub doCommunication(){

  my $ip = shift;
  my $port = shift;
  my $direction = shift;
  my $dir = 0;
  if($direction eq 'send'){ $dir = 0; }else{ $dir = 1; }

  my $velocity = int((1.0 - ($port / 65536))*127);

  my $instrument = $clientMap{$ip};
  my $tone = 33;
  my $durationKey;
  if( exists $noteSets{ $instrument } ){
    $tone = $noteSets{ $instrument }->[int( rand( @{$noteSets{ $instrument }} ) )];
    $durationKey = $instrument;
  }else{
    $tone = $noteSets{ 'default' }->[int( rand( @{$noteSets{ 'default' }} ) )];
    $durationKey = 'default';
  }

  my $duration = &getDuration($durationKey);

  #API is "instrument velocity tone duration"
  print "".$clientMap{$ip}." $velocity $duration $tone\n";
  
}

sub getDuration() {
  my $key = shift;
  my ($min, $max) = ($minDurations{$key}, $maxDurations{$key});

  return $min + int(rand($max - $min));
}

sub directionTransform(){
  my $noteSpec = shift;

  if( $noteSpec =~ /,/ ){
    my @notes = split /,/, $noteSpec;
    $_ = $_ + 12 for @notes;
    return join ',',@notes;
  }else{
    return $noteSpec + 12;
  }
}

sub addClientIfNew(){

  my $srcip = shift;
  my $dstip = shift;

  if( !exists $clientMap{$srcip} ){
    $clientMap{$srcip} = $num_clients++;
  } 

  if( !exists $clientMap{$dstip} ){
    $clientMap{$dstip} = $num_clients++;
  } 

}
