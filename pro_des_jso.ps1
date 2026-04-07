conda deactivate
# 2. 正确激活 ESP-IDF 环境
C:\AccountNCA\Apply\Espressif\frameworks\esp-idf-v5.5.4\export.ps1
# 3. 验证环境
idf.py --version

idf.py reconfigure

# echo "已恢复project_description.json 文件。"