use Ymagine;

use Cwd qw(abs_path);
use File::Basename qw(dirname);

my $moddir = dirname (abs_path(__FILE__));
my $testdir = abs_path("$moddir/..");
my $datadir = "$testdir/data";

my $srcfile = "$datadir/resize/orig.jpg";
my $dstfile = "";
my $options = {
  width => 300,
  height => 300,
  scale => 'crop',
  quality => -1,
  sharpen => 0.3,
  subsample => -1,
  meta => 'none',
  rotate => 0.0
};

for my $fmt ( 'jpg', 'webp', 'png' ) {
  $options->{format} = $fmt;
  $dstfile = "out.$options->{format}";
  $rc = Ymagine::transcode($srcfile, $dstfile, $options);
  if ($rc == 0) {
    print "Transcode passed, output is $dstfile\n";
  } else {
    print "Transcode failed, return code $rc\n";
  }
}
