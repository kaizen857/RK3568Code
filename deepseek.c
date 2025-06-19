#include <stdio.h>
#include <string.h>

char result[4096];

void shift_string_left(char *str, size_t k)
{
    size_t len = strlen(str);
    if (k >= len)
    {
        // 若 k 超过字符串长度，直接清空字符串
        str[0] = '\0';
        return;
    }
    // 计算需要移动的字节数（从第 k 个字符到末尾）
    size_t bytes_to_move = len - k + 1; // +1 包含结尾的 '\0'
    // 使用 memmove 前移字符串
    memmove(str, str + k, bytes_to_move);
}

int main()
{
    char question[1024];
    char buffer[1024];
    char buffer1[4096];

    while (1)
    {
        bzero(question, sizeof(question));
        bzero(buffer, sizeof(buffer));
        bzero(buffer1, sizeof(buffer1));
        bzero(result, sizeof(result));

        printf("请输入问题:");
        scanf("%s", question);

        // 拼接指令
        char cmd[4096] = {0};
        sprintf(cmd, "python3 deepseek.py %s", question);

        // 加载chatglm模型
        FILE *fp = popen(cmd, "r");
        if (fp == NULL)
        {
            perror("无法加载模型\n");
            return 1;
        }

        // 读取chatglm 模型的输出内容
        while (1)
        {
            if (fgets(buffer, sizeof(buffer), fp) == NULL)
            {
                printf("加载完毕\n");
                break;
            }
            strcat(buffer1, buffer);
        }

        // printf("search_results： %s\n", search_results);
        char *p = strstr(buffer1, "</think>");
        shift_string_left(p, 9);
        strcpy(result, p);
        printf("最终结果: %s\n", result);

        pclose(fp);
    }
}