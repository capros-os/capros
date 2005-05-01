#!/usr/bin/perl -W
use FileHandle;

sub splitLine($)
{
  my ($fileName) = @_;
  my ($fh, $str, $lastLine, @array);
  
  $fh = new FileHandle $fileName or die "Cannot open $fileName: $!";
  while (defined($str = <$fh>))
  {
    $lastLine = $str;
  }
  @array = split(/\s+/, $lastLine, 2);
  return @array;
}

{
  my ($fileName, $fileNames, %traces, $location, $rest);

  for $fileName (@ARGV)
  {
    ($location, $rest) = splitLine($fileName);
    if (exists($traces{$location}))
    {
      push @{$traces{$location}}, $fileName;
    }
    else
    {
      $traces{$location} = [ $fileName ];
    }
  }
  for $location (keys %traces)
  {
    $fileNames = $traces{$location};
    print $location, "  ", scalar(@$fileNames), "\n";
    for $fileName (@$fileNames)
    {
      print "  ", $fileName, "\n";
    } 
  }
}
