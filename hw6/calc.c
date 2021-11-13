#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "logger.h"
#include <assert.h>
#include "calc.h"

#define SIZE_OF_MAP 10

/// This struct contains a node of a dictionary that maps a string to an int.
struct Map
{
    char key[20];
    int value;
};

/// This struct contains the general program state
struct Calc
{
    struct Map variables[SIZE_OF_MAP];
};

// -- < Auxiliar functions > ---------------

///     Returns true if precedence of operator o1 is more or equal than the one of o2
int precedence(char o1, char o2)
{
    if (((o1 == '+') || (o1 == '-')) && ((o2 == '*') || (o2 == '/')))
        return 0;

    else if (o1 == '=' && ((o2 == '+') || (o2 == '-') || (o2 == '*') || (o2 == '/')))
        return 0;
    else
        return 1;
}

///     Performs arithmethic operations and returns the result.
int arithmethicOp(int n1, int n2, char op)
{
    switch (op)
    {
    case '+':
        return n1 + n2;
    case '-':
        return n1 - n2;
    case '*':
        return n1 * n2;
    case '/':
        return n1 / n2;
    }

    assert(0 && "Undefined operator found in arithmethicOp");
    return 0;
}

int getHash(char *key)
{
    size_t k = 0;

    for (size_t i = 0; i < strlen(key); i++)
    {
        k = k + (size_t)key[i];
    }
    return (k % SIZE_OF_MAP);
}

// -- < State manipulation functions > ---------------

/// Summary:
///     Create a Calc object and initialize it

struct Calc *calc_create()
{
    LOG_INFO("Creating Calc Object\n\n");
    return (struct Calc *)malloc(sizeof(struct Calc));
}

///     Destroy Calc object
void calc_destroy(struct Calc *calc)
{
    LOG_INFO("Destroying Calc Object\n\n");
    free(calc);
}

///     Given an expression, evaluates it on the calculator and return the result
int calc_eval(struct Calc *calc, const char *expr, int *result)
{
    LOG_INFO("Evaluating Expression in Calc Object\n\n");

    char operatorStack[20];
    int numberStack[25];
    char variablesStack[5][25];
    char first = ' ';

    int head = -1;
    int headOp = -1;
    int headVariable = -1;

    size_t i = 0;
    while (expr[i] != '\0') // Iterate character by character until the expression ends
    {

        if (isspace(expr[i]) != 0) // As long as the character is different from space
        {
            i++;
            continue;
        }

        if (isalpha(expr[i]) != 0) // If it is a letter of the alphabet
        {
            char l[20] = "";

            while (i < strlen(expr) && isalpha(expr[i]) != 0) // Iterate character by character until the variable name is complete.
            {
                strncat(l, &expr[i], 1);
                i++;
            }

            if (strlen(expr) - i == 2 && strcmp(calc->variables[getHash(l)].key, l) != 0) // If there is no operator but a variable
            {
                LOG_ERROR("Undefined variable error: The given variable does not exist in the calculator. \n");
                return FAILURE;
            }

            if (strcmp(calc->variables[getHash(l)].key, l) != 0 && i < strlen(expr))
            {
                strcpy(calc->variables[getHash(l)].key, l); // store variable name in the calculator
                headVariable++;
                strcpy(variablesStack[headVariable], l);
            }
            else
            {

                if (headVariable < 0)
                {
                    headVariable++;
                    strcpy(variablesStack[headVariable], l);
                }
                else
                {
                    head++;
                    numberStack[head] = calc->variables[getHash(l)].value;
                }
            }

            i--;
        }
        else if (expr[i] >= '0' && expr[i] <= '9') // If it is a number of the alphabet
        {                                          // If it is a number

            int n = 0;

            while (i < strlen(expr) && expr[i] >= '0' && expr[i] <= '9') // Iterate digit by digit until the number is complete.
            {
                n = (n * 10) + (expr[i] - '0');
                i++;
            }
            numberStack[++head] = n; // store variable name in a stack of numbers
            i--;
        }
        else
        {
            while (headOp >= 0 && precedence(operatorStack[headOp], expr[i])) // If there is an operator and its precedence is bigger that the one of the current character in the expression.
            {
                if (operatorStack[headOp - 1] != '=') // If there are no variables in the operation, performs arithmethic operation.
                {

                    if ((head < 0 && headVariable < 0) || (head == headOp && headVariable < 0 && headOp >= 0) || (head < 0 && headVariable == headOp && headOp >= 0))
                    {
                        LOG_ERROR("Arity error: there is a binary arity operator. An argument is missing.\n");
                        return FAILURE;
                    }

                    int n2 = numberStack[head--];
                    int n1 = numberStack[head--];
                    first = operatorStack[headOp--];

                    if (n2 == 0 && first == '/')
                    {
                        LOG_ERROR("Arithmetic error: Cannot divide by zero.\n");
                        return FAILURE;
                    }

                    numberStack[head++] = arithmethicOp(n1, n2, first);
                }
                else
                {
                    headOp--;
                    int n1;
                    if (headVariable == 0) // If a value is assigned to a variable directly
                    {
                        n1 = numberStack[head];
                    }
                    else //If a value is assigned to a variable by means of another variable stored in the calculator
                    {
                        n1 = calc->variables[getHash(variablesStack[headVariable])].value;
                        numberStack[head++] = n1;
                    }
                    calc->variables[getHash(variablesStack[headVariable - 2])].value = n1;
                }
            }
            operatorStack[++headOp] = expr[i];
        }

        i++;
    }

    while (headOp >= 0) // When there are no preceding operators left
    {
        if (operatorStack[headOp] != '=')
        {
            if ((head < 0 && headVariable < 0) || (head == headOp && headVariable < 0 && headOp >= 0) || (head < 0 && headVariable == headOp && headOp >= 0))
            {
                LOG_ERROR("Arity error: there is a binary arity operator. An argument is missing.\n");
                return FAILURE;
            }

            int n2 = numberStack[head--];
            int n1 = numberStack[head--];
            first = operatorStack[headOp--];

            if (n2 == 0 && first == '/')
            {
                LOG_ERROR("Arithmetic error: Cannot divide by zero.\n");
                return FAILURE;
            }

            if (n2 == 0 && first == '/')
            {
                LOG_ERROR("Arithmetic error: Cannot divide by zero.\n");
                return FAILURE;
            }

            numberStack[++head] = arithmethicOp(n1, n2, first);
        }
        else
        {
            headOp--;
            int n1;
            if (headVariable == 0) // If a value is assigned to a variable directly
            {
                n1 = numberStack[head];
            }
            else //If a value is assigned to a variable by means of another variable stored in the calculator
            {
                n1 = calc->variables[getHash(variablesStack[headVariable])].value;
                numberStack[head++] = n1;
            }
            calc->variables[getHash(variablesStack[0])].value = n1;
        }
    }

    if (first != '=' && headVariable >= 0) // If there is no operator but a variable
    {
        if (strcmp(calc->variables[getHash(variablesStack[headVariable])].key, variablesStack[headVariable]) != 0)
        {
            LOG_ERROR("Undefined variable error: The given variable does not exist in the calculator. \n");
            return FAILURE;
        }

        *result = calc->variables[getHash(variablesStack[headVariable])].value;
        return SUCCESS;
    }

    *result = numberStack[head];
    return SUCCESS;
}