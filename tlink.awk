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
