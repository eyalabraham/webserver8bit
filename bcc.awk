#
# tasm.awk
#
#  awk script to extract and print assembly errors in a format that Eclipse
#  recognizes and displays
#  convert:
#    **Error** l:\webserve\src\_lmte.asm(114) Undefined symbol: _TEXT
#  to:
#    _lmte.asm:114:Undefined symbol: _TEXT
#
BEGIN       {
                FILENAME="{none}";
                ERRLINE=-1;
                ERRTEXT="{none}";
            }

            {
                if ( $1 == "Error" ||
                     $1 == "Warning"  )
                {
                    FILENAME=basename($2);
                    ERRLINE=$3
                    sub(/:$/,"",ERRLINE);
                    ERRTEXT=$0
                    printf("%s:%s: %s\n", FILENAME, ERRLINE, ERRTEXT);   # print Eclipse format error
                }
                else
                    print $0;
            }

END         {
            }
#
# remove all characters left of the file name
# return base files name
#
function basename(file)
{
    sub(/.*\\/, "", file);
    return file;
}
