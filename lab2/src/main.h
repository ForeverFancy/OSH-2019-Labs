
struct SimpleCommand
{
    int number_of_available_arguments;
    int number_of_arguments;

    int redirect;
    int redirect_out_append;
    int redirect_out_cover;
    int redirect_in;
    int redirect_error;

    int redirect_combination;

    char *file_src_in;
    char *file_src_out;

    char *combination_file1;
    char *combination_file2;

    int pipe;
    int background;
    char *arguments[1024];
    //SimpleCommand();
    //void insertArgument(char * argument);
};

