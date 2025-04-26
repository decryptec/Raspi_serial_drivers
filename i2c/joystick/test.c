#include <stdio.h>
#include <stdlib.h>

#define answer_attr "/sys/class/my_class/my_dev0/answer"
#define text_attr "/sys/class/my_class/my_dev0/text"

int main() {
    FILE *sysfs_file;
    char buffer[256];
    char value_to_write[256];

    // Answer R/W
    // Reading the attribute
    sysfs_file = fopen(answer_attr, "r");
    if (sysfs_file == NULL) {
        perror("Failed to open sysfs file");
        exit(EXIT_FAILURE);
    }
    fscanf(sysfs_file, "%255s", buffer);
    fclose(sysfs_file);
    printf("Current value: %s\n", buffer);

    // Prompt user for the value to write
    printf("Enter the value to write: ");
    scanf("%255s", value_to_write);

    // Writing to the attribute
    sysfs_file = fopen(answer_attr, "w");
    if (sysfs_file == NULL) {
        perror("Failed to open sysfs file");
        exit(EXIT_FAILURE);
    }
    fprintf(sysfs_file, "%s", value_to_write);  // Write user-provided value
    fclose(sysfs_file);
    printf("Write answer successful!\n");

    // Text R/W
    // Reading attribute
    sysfs_file = fopen(text_attr, "r");
    if (sysfs_file == NULL) {
        perror("Failed to open sysfs file");
        exit(EXIT_FAILURE);
    }
    fscanf(sysfs_file, "%255s", buffer);
    fclose(sysfs_file);
    printf("Current text: %s\n", buffer);  // Changed from fprintf to printf

    // Prompt user for new text to write
    printf("Enter new text to write: ");  // Fixed typo in prompt message
    scanf("%255s", value_to_write);

    // Writing to the attribute
    sysfs_file = fopen(text_attr, "w");
    if (sysfs_file == NULL) {
        perror("Failed to open sysfs file");
        exit(EXIT_FAILURE);
    }
    fprintf(sysfs_file, "%s", value_to_write);  // Write user-provided value
    fclose(sysfs_file);
    printf("Write text successful!\n");

    return 0;    
}
