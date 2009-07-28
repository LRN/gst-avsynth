#!/usr/bin/perl
# Save this to your disk as "AssyXlate.pl"
########################################################################
# 
# AssyXlate.pl -- MSVC => GCC inline assy translator
# 
$version = "1.03  (11/27/08)";
#
# by Randy Langer
# 
# Project homepage: www.technicrafts.com/AssyXlate.htm
# 
# Use of this code is unrestricted -- enjoy!
# 
########################################################################

print STDERR "AssyXlate.pl  $version\n\n",
    "    Be sure to add in the '-masm=intel' cmdline switch and the \n",
    "    'GCC__' preprocessor define to the GCC project.\n\n";

$outputBoth = 0;
$scopeOrd = -1;

while($i = shift @ARGV)
{
    if(lc($i) eq '-b')
    {
        $outputBoth = 1;
    }
    elsif(lc($i) eq '-s')
    {
        $scopeOrd = 0;
    }
}

%specialCaseOps =
(
    bound       => 0x0A,
    bt          => 0x0A,
    cmp         => 0x0A,
    comisd      => 0x0A,
    comiss      => 0x0A,
    div         => 0x02,
    enter       => 0x0A,
    idiv        => 0x02,
    imul        => 0x02,
    maskmovdqu  => 0x0A,
    mov         => 0x09,
    movapd      => 0x09,
    movaps      => 0x09,
    movd        => 0x09,
    movdqa      => 0x09,
    movdqu      => 0x09,
    movhpd      => 0x09,
    movhps      => 0x09,
    movlpd      => 0x09,
    movlps      => 0x09,
    movntdq     => 0x09,
    movni       => 0x09,
    movntpd     => 0x09,
    movntps     => 0x09,
    movntq      => 0x09,
    movq        => 0x09,
    movsd       => 0x09,
    movss       => 0x09,
    mul         => 0x02,
    out         => 0x0A,
    pop         => 0x01,
    push        => 0x02,
    test        => 0x0A,
    ucomisd     => 0x0A,
    ucomiss     => 0x0A,
    verr        => 0x02,
    verw        => 0x02,
    xadd        => 0x0F,
    xchg        => 0x0F,
);

%extendedClobbers =
(
    aaa         => [ '"eax"' ],
    aad         => [ '"eax"' ],
    aam         => [ '"eax"' ],
    aas         => [ '"eax"' ],
    cbw         => [ '"eax"' ],
    cwde        => [ '"eax"' ],
    cld         => [ '"cc"' ],
    cmpsb       => [ '"esi"', '"edi"' ],
    cmpsw       => [ '"esi"', '"edi"' ],
    cmpsd       => [ '"esi"', '"edi"' ],
    cmpxchg     => [ '"eax"' ],
    cmpxchg8b   => [ '"eax"', '"edx"' ],
    cpuid       => [ '"eax"', '"ebx"', '"ecx"', '"edx"' ],
    cwd         => [ '"eax"', '"edx"' ],
    cdq         => [ '"eax"', '"edx"' ],
    daa         => [ '"eax"' ],
    das         => [ '"eax"' ],
    div         => [ '"eax"', '"edx"' ],
    idiv        => [ '"eax"', '"edx"' ],
    imul        => [ '"eax"', '"edx"' ],
    insb        => [ '"memory"' ],
    insw        => [ '"memory"' ],
    insd        => [ '"memory"' ],
    lahf        => [ '"eax"' ],
#    lds         => [ '"ds"' ],
#    les         => [ '"es"' ],
#    lfs         => [ '"fs"' ],
#    lgs         => [ '"gs"' ],
#    lss         => [ '"ss"' ],
    lodsb       => [ '"eax"', '"esi"' ],
    lodsw       => [ '"eax"', '"esi"' ],
    lodsd       => [ '"eax"', '"esi"' ],
    loop        => [ '"ecx"' ],
    loope       => [ '"ecx"' ],
    loopz       => [ '"ecx"' ],
    loopne      => [ '"ecx"' ],
    loopnz      => [ '"ecx"' ],
    maskmovdqu  => [ '"memory"' ],
    maskmovq    => [ '"memory"' ],
    movsb       => [ '"memory"', '"esi"', '"edi"' ],
    movsw       => [ '"memory"', '"esi"', '"edi"' ],
    movsd       => [ '"memory"', '"esi"', '"edi"' ],
    mul         => [ '"eax"', '"edx"' ],
    outsb       => [ '"esi"' ],
    outsw       => [ '"esi"' ],
    outsd       => [ '"esi"' ],
    popa        => [ '"eax"', '"ebx"', '"ecx"', '"edx"', '"esi"', '"edi"' ],
    popf        => [ '"cc"' ],
    rdtsc       => [ '"eax"', '"edx"' ],
    scasb       => [ '"edi"' ],
    scasw       => [ '"edi"' ],
    scasd       => [ '"edi"' ],
    std         => [ '"cc"' ],
    stosb       => [ '"memory"', '"edi"' ],
    stosw       => [ '"memory"', '"edi"' ],
    stosd       => [ '"memory"', '"edi"' ],
);

%regNames =
(
    al      => "eax",
    ah      => "eax",
    ax      => "eax",
    eax     => "eax",
    bl      => "ebx",
    bh      => "ebx",
    bx      => "ebx",
    ebx     => "ebx",
    cl      => "ecx",
    ch      => "ecx",
    cx      => "ecx",
    ecx     => "ecx",
    dl      => "edx",
    dh      => "edx",
    dx      => "edx",
    edx     => "edx",
    si      => "esi",
    esi     => "esi",
    di      => "edi",
    edi     => "edi",
    bp      => "ebp",
    ebp     => "ebp",
    sp      => "esp",
    esp     => "esp",
    mm0     => "mm0",
    mm1     => "mm1",
    mm2     => "mm2",
    mm3     => "mm3",
    mm4     => "mm4",
    mm5     => "mm5",
    mm6     => "mm6",
    mm7     => "mm7",
    xmm0    => "xmm0",
    xmm1    => "xmm1",
    xmm2    => "xmm2",
    xmm3    => "xmm3",
    xmm4    => "xmm4",
    xmm5    => "xmm5",
    xmm6    => "xmm6",
    xmm7    => "xmm7",
    mxcsr   => "mxcsr",
);

%reserveds =
(
    ds      => "ds",
    es      => "es",
    ss      => "ss",
    cs      => "cs",
    fs      => "fs",
    gs      => "gs",
    qword   => "qword",
    dword   => "dword",
    word    => "word",
    byte    => "byte",
    ptr     => "ptr",
    offset  => "offset",
);

while(<>)
{
    if(/^\s*__asm/)
    {
        HandleAssyBlock();
    }
    else
    {
        print;
    }
}

sub HandleAssyBlock
{

my  ($accum, $i, $j, $k, $m, $t, $label, $mnemonic, $endmark);
my  ($tabs, $line, $cmnt, $unclob);
my  @operands;
my  @pieces;
my  %clobbers;
my  %xargs;
my  %scopedLabels;

    if($outputBoth)
    {
        print '#if !defined(GCC__)', "\n";
        print;
    }

    while(<>)
    {
        if($outputBoth)
        {
            print;
        }

        if($cmnt)
        {
            if(/\*\//)
            {
                s/.*\*\///;
                $cmnt = 0;
            }
            else
            {
                next;
            }
        }

        1 while(s/\/\*.*\*\///);

        if(/\/\*/)
        {
            $cmnt = 1;
            s/\/\*.*//;
        }

        if(/\/\/\{GCC_UNCLOBBERED\(([^)]+)\)\}/)
        {
            $unclob .= ' ' . $1;
        }

        $t = /\/\/\{MSVC_ONLY\}/;
        s/\/\/.*//;

        if(/^\s*#/)
        {
            $accum .= $_;
            next;
        }

        chomp;
        s/^\s*\{//;

        if(/\}/)
        {
            $endmark = 1;
            s/\}.*//;
        }

        s/^\s+//;

        if($_ eq "" or $t)
        {
            $accum .= "\n";
            goto BLK_END if($endmark);
            next;
        }

        if(/(\w+)\s*:(.*)/)
        {
            $label = $1 . ': ';
            $scopedLabels{$1} = 1;
            $_ = $2;
            s/^\s+//;
        }
        else
        {
            $label = "";
        }

        if(/^(rep|repe|repz|repne|repnz)(\s.*)/)
        {
            $accum .= "\t" . '"' . $label . "\t" . $1 . '\n"' . "\n";
            $_ = $2;
            $label = "";
            s/^\s+//;
            $clobbers{'"ecx"'} = 1;
        }

        if(/^(\w+)(.*)/)
        {
            $m = lc($mnemonic = $1);
            $_ = $2;
            s/^\s+//;
            $tabs = (length($m) < 4 ? "\t\t" : "\t");

            if(exists $extendedClobbers{$m})
            {
                for($j = $extendedClobbers{$m}, $i = 0; $i < @{$j}; ++$i)
                {
                    $clobbers{$j->[$i]} = 1;
                }
            }

            if($_ ne "")
            {
                s/^\s+//;
                s/\s+$//;
                @operands = split /\s*,\s*/, $_;

                if(@operands == 1 and ($m eq 'call' or $m =~ /^j/ or 
                    $m =~ /^loop/))
                {
                    $accum .= "\t" . '"' . $label . "\t" . $mnemonic . 
                        $tabs . $_ . '\n"' . "\n";

                    goto BLK_END if($endmark);
                    next;
                }

                for($i = 0; $i < @operands; ++$i)
                {
                    if(exists $specialCaseOps{$m})
                    {
                        $t = (($specialCaseOps{$m} >> ($i * 2)) & 3);
                    }
                    else
                    {
                        $t = ($i ? 2 : 3);
                    }

                    $k = lc $operands[$i];
                    @pieces = split /(\w+)/, $operands[$i];
                    shift @pieces if($pieces[0] eq "");

                    if($t & 1)
                    {
                        if(@pieces > 1)
                        {
                            $clobbers{'"memory"'} = 1;
                        }
                        elsif(exists $regNames{$k} and $k ne "ebp")
                        {
                            $clobbers{'"' . $k . '"'} = 1;
                        }
                    }

                    for($j = 0; $j < @pieces; ++$j)
                    {
                        $k = lc $pieces[$j];

                        if($k =~ /^\w+$/ and $k !~ /^\d/ and
                            not exists $regNames{$k} and
                            not exists $reserveds{$k})
                        {
                            $xargs{$pieces[$j]} |= $t;
                            $pieces[$j] = '%[' . $pieces[$j] . ']';
                        }
                    }

                    $operands[$i] = join "", @pieces;
                }

                $k = join ",", @operands;
            }
            else
            {
                $k = "";
            }
        }
        else
        {
            $mnemonic = "";
            $k = "";
        }

        $accum .=  "\t" . '"' . $label . "\t" . $mnemonic . $tabs . $k . 
            '\n"' . "\n";

        BLK_END:
        if($endmark)
        {
            $j = $k = "";
            $accum =~ s/\s+\\n/\\n/g;
            
            if($unclob ne "")
            {
                @pieces = split /\s+/, $unclob;
                shift @pieces if($pieces[0] eq "");

                while($i = shift @pieces)
                {
                    delete $clobbers{lc($i)};

                    if(exists $xargs{$i} and $xargs{$i} & 1)
                    {
                        $xargs{$i} = 2;
                    }
                }
            }
            
            foreach $i (keys %xargs)
            {
                if($xargs{$i} & 1)
                {
                    if($j ne "")
                    {
                        $j .= ",\n\t\t";
                    }
                    else
                    {
                        $j = "\t:\t";
                    }
            
                    $j .= '[' . $i . '] "=m" (' . $i . ')';
                }
            
                if($xargs{$i} == 2)
                {
                    if($k ne "")
                    {
                        $k .= ",\n\t\t";
                    }
                    else
                    {
                        $k = "\t:\t";
                    }
            
                    $k .= '[' . $i . '] "m" (' . $i . ')';
                }
            }
            
            if($j ne "")
            {
                $j .= "\n";
            }
            
            if($k ne "")
            {
                $k .= "\n";
            }

            $i = join ", ", keys %clobbers;
            
            if($i)
            {
                $i = "\t:\t" . $i;
            
                if($k eq "")
                {
                    $k = "\t:\n";
                }
            }
            
            if($k ne "" and $j eq "")
            {
                $j = "\t:\n";
            }

            $accum .= $j . $k . $i . "\n\t);\n";

            if($outputBoth)
            {
                print '#else', "\n";
            }

            if($scopeOrd >= 0)
            {
                foreach $i (keys %scopedLabels)
                {
                    $j = sprintf "\$%d_%s", $scopeOrd, $i;
                    $accum =~ s/\b$i\b/$j/g;
                }

                ++$scopeOrd;
            }

            print "\t__asm__ __volatile__\n\t(\n", $accum;

            if($outputBoth)
            {
                print '#endif', "\n";
            }

            return;
        }
    }
}


