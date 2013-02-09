#ifndef PARSE_H

#define PARSE_H

int parse_action_line();
int parse_line(char *line);
#if 0 
char *parse_label_definition(char **line, char *label);
char *parse_label(char **line, char *label);
void skip_whitespace(char **line);
#endif 
int is_string_directive(char *line);
int is_data_directive(char *line);
int is_valid_instruction(char *line);

#endif /* end of include guard: PARSE_H */
