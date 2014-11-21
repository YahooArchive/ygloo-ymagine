package Ymagine;

our $VERSION = '0.01';

$Ymagine::scale_letterbox = 0;
$Ymagine::scale_crop = 1;

$Ymagine::format_unknown = 0;
$Ymagine::format_jpeg    = 1;
$Ymagine::format_webp    = 2;
$Ymagine::format_png     = 3;
$Ymagine::format_gif     = 4;

use XSLoader;
XSLoader::load('yahoo_ymaginexs', $VERSION);

use POSIX "fmod";

sub Ymagine::transcode {
  # get total number of arguments passed.
  my $n = scalar(@_);

  die "Too few arguments, should be transcode infile outfile ?options?" unless $n >= 2;

  my $infile = $_[0];
  my $outfile = $_[1];

  my $quality = -1;
  my $subsample = -1;
  my $width = -1;
  my $height = -1;
  my $scalemode = 'letterbox';
  my $format = '';
  my $sharpen = 0;
  my $meta = -1;
  my $rotate = 0;

  if ($n > 2) {
    my $options = $_[2];
    if (exists $options->{width}) {
      $width = $options->{width};
    }
    if (exists $options->{height}) {
      $height = $options->{height};
    }
    if (exists $options->{format}) {
      $format = $options->{format};
    }
    if (exists $options->{scale}) {
      $scalemode = $options->{scale};
    }
    if (exists $options->{sharpen}) {
      $sharpen = $options->{sharpen};
    }
    if (exists $options->{quality}) {
      $quality = $options->{quality};
    }
    if (exists $options->{subsample}) {
      $subsample = $options->{subsample};
    }
    if (exists $options->{meta}) {
      $meta = $options->{meta};
    }
    if (exists $options->{rotate}) {
      $rotate = $options->{rotate};
    }
  }

  my $iscalemode = $scale_letterbox;
  if ($scalemode eq 'crop') {
    $iscalemode = $scale_crop;
  } elsif ($scalemode eq 'letterbox') {
    $iscalemode = $scale_letterbox;
  } else {
    # Invalid crop mode
    $iscalemode = $scale_letterbox;
  }

  my $isharpen = 0;
  if ($sharpen > 0.0) {
    if ($sharpen >= 1.0) {
      $isharpen = 100;
    } else {
      $isharpen = int($sharpen * 100.0);
    }
  }

  my $iformat = $format_unknown;
  if ($format eq 'jpg' || $format eq 'jpeg') {
    $iformat = $format_jpeg;
  } elsif ($format eq 'webp') {
    $iformat = $format_webp;
  } elsif ($format eq 'png') {
    $iformat = $format_png;
  } elsif ($format eq 'gif') {
    $iformat = $format_gif;
  } elsif ($format eq '' || $format eq '-') {
    $iformat = $format_unknown;
  } else {
    # Invalid image format
    $iformat = $format_unknown;
  }

  my $irotate = 0;
  if ($rotate != 0.0) {
    $irotate = int(fmod($rotate, 360.0));
  }

  my $imeta = -1;
  if ($meta eq 'none' || $meta eq '-') {
    $imeta = 0;
  } elsif ($meta eq 'comments') {
    $imeta = 1;
  } elsif ($meta eq 'all') {
    $imeta = 2;
  }

  # Call native wrapper
  return _xs_transcode ($infile, $outfile, $iformat,
			$width, $height, $iscalemode,
			$quality, $isharpen, $subsample,
		        $irotate, $meta);
}
