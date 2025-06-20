import requests
import json
import sys

def deepseek_query(prompt, model="deepseek-r1:1.5b"):
    url = "http://127.0.0.1:11434/api/chat"
    headers = {"Content-Type": "application/json"}
    
    payload = {
        "model": model,
        "messages": [
            {"role": "system", "content": "你是一位专业的AI助手"},
            {"role": "user", "content": prompt}
        ],
        "temperature": 0.7,
        "max_tokens": 2048,  # 增大最大生成长度
        "stream": False       # 显式关闭流式传输
    }
 
    try:
        response = requests.post(url, headers=headers, json=payload, timeout=30)
        response.raise_for_status()
        
        # 标准JSON解析方式
        result = response.json()
        return result.get("message", {}).get("content", "").strip()
    
    except requests.exceptions.RequestException as e:
        return f"请求失败: {str(e)}"
    except json.JSONDecodeError:
        return "响应格式异常，请检查服务状态"
 
# 使用示例（非流式）
#获取命令行参数
if len(sys.argv) < 2:
    print("Usage: python chatgml.py [your_prompt]")
    sys.exit(1)

prompt1 = sys.argv[1]  
if __name__ == "__main__":
    user_input = prompt1
    print("AI思考中...")
    full_response = deepseek_query(user_input)
    
    if full_response.startswith("请求失败"):
        print(f"错误: {full_response}")
    else:
        print(full_response)
        print("\n生成完成")