#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

struct node {
    int value;
    struct node *next;
};

struct node *push(struct node *head, int value)
{
    struct node *n = malloc(sizeof(*n));
    if(n == 0) {
        printf("out of memory\n");
        exit(1);
    }
    n->value = value;
    n->next = head;
    return n;
}

void remove_value(struct node **head, int value) {
    struct node *current = *head;
    struct node *prev = 0;

    while (current != 0) {
        if (current->value == value) {
          if (prev == 0) {
                *head = current->next;
            } else {
                prev->next = current->next;
            }

            free(current);
            return;
        }

        prev = current;
        current = current->next;
    }
}

void print_list(struct node *head)
{
    while(head) {
        printf(" %d", head->value);
  head = head->next;
    }    
    printf("\n");
}

void free_list(struct node *head) {
    struct node *t;

    while(head) {
        t = head->next;
        free(head);
        head = t;
    }
}

int main(void)
{
    struct node *head = 0;

    for(int i = 1; i <= 8; ++i) {
      head = push(head, i);
    }
    printf("initial list:\n");
    print_list(head);

    remove_value(&head, 3);
    printf("list with removed value:\n");
    print_list(head);

    head = push(head, 7331);
    printf("list with new value:\n");
    print_list(head);

    free_list(head);
    printf("memory is freed\n");
    exit(0);
}
