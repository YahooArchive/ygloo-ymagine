#!/bin/sh
# the next line restarts using tclsh \
exec tclsh "$0" ${1+"$@"}

proc usage {} {
  puts "ccvserialize <directory>"
}

proc zigzag32 {n} {
  set isneg [expr {$n < 0}]
  if {$isneg} {
    set m [expr {int(0xffffffff)}]
  } else {
    set m [expr {int(0)}]
  }

  set ref [expr {int(($n << 1) & 0xfffffffe)}]
  set z [expr {$ref ^ $m}]

  return $z
}

proc unzigzag32 {n} {
  set isneg [expr {$n & 1}]
  if {$isneg} {
    set m [expr {int(0x7fffffff)}]
    set z [expr {-1 - (($n >> 1) & $m)}]
  } else {
    set m [expr {int(0)}]
    set z [expr {(($n ^ $m) >> 1)}]
  }

  return $z
}

proc varint {args} {
  set result ""
  foreach n $args {
    while {1} {
      set l [expr {$n & 0x7f}]
      set n [expr {$n >> 7}]

      if {$n > 0} {
	set l [expr {$l | 0x80}]
      }
      append result [binary format c $l]

      if {$n <= 0} {
	break
      }
    }
  }

  return $result
}

proc sint32 {args} {
  set result ""
  foreach n $args {
    append result [varint [zigzag32 $n]]
  }

  return $result
}

proc vfloat {args} {
  set result ""

  foreach n $args {
    #append result [binary format r $n]
    append result [binary format i $n]
  }

  return $result
}

proc main {args} {
  if {[llength $args] <= 0} {
    usage
    return
  }

  set d [lindex $args 0]
  if {![file exist $d] || ![file isdirectory $d]} {
    error "Input '$d' is not a valid directory"
  }
  if {![file exist [file join $d cascade.txt]]} {
    error "Directory '$d' doesn't contain valid cascade definition"
  }

  set name [file tail $d]

  set ifmt "i"
  #set ffmt "r"
  set ffmt "i"
  set cfmt "c"

  set fd [open [file join $d cascade.txt] r]
  set b [read $fd]
  close $fd

  set result ""
  set cresult ""

  scan $b "%d %d %d" nstages width height
  append result [binary format "${ifmt}${ifmt}${ifmt}" $nstages $width $height]
  append cresult [varint $nstages $width $height]

  for {set i 0} {$i < $nstages} {incr i} {
    # puts "Reading stage [expr {$i+1}]/$nstages"

    set fd [open [file join $d stage-$i.txt] r]
    # Number of feature
    scan [gets $fd] "%d" nfeatures
    # Threshold (float)
    scan [gets $fd] "%d" threshold

    append result [binary format "${ifmt}${ffmt}" $nfeatures $threshold]
    append cresult [varint $nfeatures] [vfloat $threshold]

    set features ""
    set alphas ""
    
    for {set j 0} {$j < $nfeatures} {incr j} {
      scan [gets $fd] "%d" fsize
      append cresult [varint $fsize]

      set pxbuf ""
      set pybuf ""
      set pzbuf ""
      set nxbuf ""
      set nybuf ""
      set nzbuf ""

      for {set k 0} {$k < 8} {incr k} {
	if {$k < $fsize} {
	  # Read px py pz
	  scan [gets $fd] "%d %d %d" px py pz
	  # Read nx ny nz
	  scan [gets $fd] "%d %d %d" nx ny nz

	  append cresult [sint32 $px $py $pz $nx $ny $nz]
	} else {
	  set px 0
	  set py 0
	  set pz 0
	  set nx 0
	  set ny 0
	  set nz 0
	}

	#puts "PX=$px"

	append pxbuf [binary format $ifmt $px]
	append pybuf [binary format $ifmt $py]
	append pzbuf [binary format $ifmt $pz]
	append nxbuf [binary format $ifmt $nx]
	append nybuf [binary format $ifmt $ny]
	append nzbuf [binary format $ifmt $nz]
      }

      append features [binary format "${ifmt}" $fsize]
      append features $pxbuf $pybuf $pzbuf $nxbuf $nybuf $nzbuf
      
      # Read alpha1 alpha2 (float)
      scan [gets $fd] "%d %d" a1 a2
      append alphas [binary format "${ffmt}${ffmt}" $a1 $a2]
      append cresult [vfloat $a1 $a2]
    }
    close $fd

    append result $features
    append result $alphas
  }

  # Use the compact encoding by default
  set result $cresult

  set csize [string length $result]
  binary scan $result {c*} bytes

  puts "/* $name */"
  puts "static const int ${name}_size = $csize;"
  puts -nonewline "static const unsigned char ${name}_data\[$csize\] = {"

  set i 0
  foreach b $bytes {
    set u [expr {($b + 0x100) % 0x100}]
    if {$i > 0} {
      puts -nonewline ","
    }
    if {($i % 16) == 0} {
      puts -nonewline "\n  "
    }
    puts -nonewline [format "0x%02x" $u]
    incr i
  }
  puts -nonewline "\n};\n"

  if {1} {
    set fd [open ${name}.bin w]
    fconfigure $fd -translation binary -encoding binary
    puts -nonewline $fd $result
    close $fd
  }

  return
}

proc test {} {
  foreach n {0 -1 1 -2 -99999 2147483647 -2147483648} {
    set z [zigzag32 $n]
    set n2 [unzigzag32 $z]
    puts "$n => $z => $n2"

    if {$n != $n2} {
      error "zigzag of $n failed"
    }
  }

  foreach n {0 32 127 128 256} {
    puts "$n => [string length [varint $n]]"
  }

  return
}

eval [list main] $argv
