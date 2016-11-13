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
                if ( $1 == "**Error**" )
                {
                    OPEN=index($2,"(");                                 # find braces
                    CLOSE=index($2,")");
                    ERRLINE=substr($2,OPEN+1,CLOSE-OPEN-1);             # and extract error line number
                    FILENAME=basename(substr($2,1,OPEN-1));
                    CLOSE=index($0,")");                                # extract error text
                    ERRTEXT=substr($0, CLOSE+2);
                    printf("%s:%s: Error - %s\n", FILENAME, ERRLINE, ERRTEXT);   # print Eclipse format error
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
