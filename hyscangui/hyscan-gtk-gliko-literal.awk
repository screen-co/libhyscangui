BEGIN{print"const char *fragmentShaderSource = "};
{print "\""$0"\\n\"";}
END{print";"}
