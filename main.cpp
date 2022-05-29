#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <fstream>

FILE* fo; // 全局变量fo

/** 宏定义 **/
#define Page_size 4*1024 // 页大小设为4KB
#define Index_page_size 500 // 索引页最多有500个key
#define Kid_page_size 7 // 叶子页最多有7个key

using namespace std;

/* 文件头结构体 */
typedef struct {
	int steps; // B+树的阶数
	unsigned int pageSize; // 页大小
	unsigned int pageCount; // 已存页的数量
	unsigned int rootPagePla; // b+树根页面所在位置，它的值是相对于文件头部的偏移值。
}File_head;

/* 索引页结构体 */
typedef struct {
	int pageType; // 页面类型，用于判断是索引页，还是数据页
	int keyCount; // 已存key的数量
	unsigned int parentPagePla; // 父页面所在位置
	unsigned int key[Index_page_size + 1]; // 索引值
	unsigned int kidPagePla[Index_page_size + 1]; // 子页面所在位置
}Index_page;

/* 数据结构体 */
typedef struct {
	char c1[129];
	char c2[129];
	char c3[129];
	char c4[81];
}Data;

/* 数据页结构体 */
typedef struct {
	int pageType; // 页面类型，用于判断是索引页，还是数据页
	int keyCount; // 已存key的数量
	bool isIncInsert; // 是否递增插入
	bool isDecInsert; // 是否递减插入
	unsigned int parentPagePla; // 父页面所在位置
	unsigned int prevPagePla; // 前一个页面所在位置
	unsigned int nextPagePla; // 后一个页面所在位置
	unsigned int key[Kid_page_size + 1]; // 一条数据的key值
	Data data[Kid_page_size + 1]; // 一条数据的data值
}Data_page;

/* 查找结果结构体 */
typedef struct {
	unsigned int pla; // 指向的位置
	int index; // 在页中的关键字序号
	bool isFound; // 是否找到
} Result;


/* 接口 */
bool insertData(Data data, unsigned int key); // 插入单条数据
int loadCsvData(string csv_file_name); // 以文件导入的方式插入大量数据
int searchBTIndex(Index_page* btPageBuf, unsigned int key); // 查找key在索引页中的位置
void searchDataPage(unsigned int page__pos, unsigned int key, Result* result); // 查找数据页
bool selectData(unsigned int key, vector<bool> field); // 主键等值查询数据
void selectRangeData(unsigned int left, unsigned int right, vector<bool> field); // 主键索引范围扫描查询数据
int outputCsvData(unsigned int left, unsigned int right, string csv_file_name); // 主键索引范围扫描的查询导出数据


void Stringsplit(string str, const char split, vector<string>& res) // 分割字符串
{
	istringstream iss(str);	// 输入流
	string token;			// 接收缓冲区
	while (getline(iss, token, split))	// 以split为分隔符
	{
		res.push_back(token);
	}
}

/* 创建数据文件 */
void createDataFile() {
	fo = fopen("dataRecord.ibd", "rb");
	if (fo == NULL) {

		unsigned int page_pos;

		// 申请并初始化页
		File_head* fileHead = (File_head*)malloc(sizeof(File_head)); memset(fileHead, 0, sizeof(File_head));
		Index_page* btPage = (Index_page*)malloc(sizeof(Index_page)); memset(btPage, 0, sizeof(Index_page));
		Data_page* dataPage = (Data_page*)malloc(sizeof(Data_page)); memset(dataPage, 0, sizeof(Data_page));


		// 以mode形式打开文件
		fo = fopen("dataRecord.ibd", "wb+");

			/* 初始化fileHead */
			fileHead->steps = Kid_page_size;
		fileHead->pageSize = Page_size;
		fileHead->pageCount = 3;
		fileHead->rootPagePla = 0;

		// 文件末尾开始写
		fseek(fo, 0 * Page_size, SEEK_SET);
		page_pos = ftell(fo);
		fwrite(fileHead, sizeof(File_head), 1, fo);

		/** 初始化fileHead指向的第一个btPage **/
		btPage->pageType = 1;
		btPage->keyCount = 1;
		btPage->key[0] = 0;
		// 文件末尾开始写
		fseek(fo, 1 * Page_size, SEEK_SET);
		page_pos = ftell(fo);
		fwrite(btPage, sizeof(Index_page), 1, fo);

		fileHead->rootPagePla = page_pos;

		/** 初始化btPage指向的第一个dataPage **/

		dataPage->pageType = 2;
		dataPage->keyCount = 0;
		dataPage->isIncInsert = true;
		dataPage->isDecInsert = true;
		dataPage->parentPagePla = page_pos;
		// 文件末尾开始写
		fseek(fo, 2 * Page_size, SEEK_SET);
		page_pos = ftell(fo);
		fwrite(dataPage, sizeof(Data_page), 1, fo);
		btPage->kidPagePla[0] = page_pos;

		// 指定位置开始写
		fseek(fo, 0, SEEK_SET);
		fwrite(fileHead, sizeof(File_head), 1, fo);

		// 指定位置开始写
		fseek(fo, fileHead->rootPagePla, SEEK_SET);
		fwrite(btPage, sizeof(Index_page), 1, fo);


		fclose(fo);
		free(fileHead);
		free(btPage);
		free(dataPage);
	}

	else {
		fclose(fo);
	}
}

/* 判断需要哪些字段 */
vector<bool> fieldChoice(string str) {
	string set[5] = { "a","c1","c2","c3","c4" };
	vector<bool> field(5);
	if (str == "*") {
		for (int i = 0; i < 5; i++) field[i] = true;
	}
	vector<string> fieldList;
	Stringsplit(str, ',', fieldList);	// 将子串存放到fieldList中
	for (int i = 0; i < fieldList.size(); i++) {
		for (int j = 0; j < 5; j++) {
			if (fieldList[i] == set[j])
				field[j] = true;
		}
	}
	return field;
}

int main() {
	createDataFile(); // 没有数据文件时，需要创建数据文件
	while (true)
	{
		cout << '>';
		string str;
		getline(cin, str);
		vector<string> strList;
		Stringsplit(str, ' ', strList);
		string menu = strList[0];

		/* 插入数据 */
		if (menu.compare("insert") == 0) {

			if (strList.size() == 6) { // 插入单条数据
				Data data;

				unsigned int key = stoi(strList[1]);
				strcpy(data.c1, strList[2].c_str());
				strcpy(data.c2, strList[3].c_str());
				strcpy(data.c3, strList[4].c_str());
				strcpy(data.c4, strList[5].c_str());

				clock_t start, end;
				start = clock();

				if (insertData(data, key)) {
					end = clock();
					cout << "执行插入单条数据:" << key << "，用时：" << float(end - start) / CLOCKS_PER_SEC << " 秒" << endl;
				}
			}
			else if (strList.size() == 2) { // 以文件导入的方式插入大量数据
				clock_t start, end;
				start = clock();

				cout << "开始载入数据，请稍后。。。" << endl;
				int cnt = loadCsvData(strList[1]);

				end = clock();
				cout << "数据载入完成，" << cnt << "数据被插入，用时：" << float(end - start) / CLOCKS_PER_SEC << " 秒" << endl;
			}
			else {
				cout << "输入命令错误，请重新输入" << endl;
			}
		}

		/* 查询数据 */
		else if (menu.compare("select") == 0) {
			if (strList.size() == 4) { // 主键等值查询数据
				vector<bool> field = fieldChoice(strList[1]);
				string str = strList[3];
				vector<string> strList;
				Stringsplit(str, '=', strList);

				clock_t start, end;
				start = clock();

				if (selectData(stoi(strList[1]), field)) {
					end = clock();
					cout << "执行查找数据操作，用时：" << float(end - start) / CLOCKS_PER_SEC << " 秒" << endl;
				}
				else {
					end = clock();
					cout << "emmm找不到这条数据，费时：" << float(end - start) / CLOCKS_PER_SEC << " 秒" << endl;
				}
			}
			else if (strList.size() == 8) { // 主键索引范围扫描查询数据
				vector<bool> field = fieldChoice(strList[1]);

				clock_t start, end;
				start = clock();

				selectRangeData(stoi(strList[5]), stoi(strList[7]), field);

				end = clock();
				cout << "执行索引范围扫描查询，用时：" << float(end - start) / CLOCKS_PER_SEC << " 秒" << endl;
			}

			else if (strList.size() == 9) { // 主键索引范围扫描的查询导出数据
				cout << "开始导出数据，请稍后。。。" << endl;
				clock_t start, end;
				start = clock();
				outputCsvData(stoi(strList[6]), stoi(strList[8]), strList[2]);
				end = clock();
				cout << "数据导出完成，用时：" << float(end - start) / CLOCKS_PER_SEC << " 秒" << endl;  //输出时间（单位：ｓ）
			}
			else {
				cout << "输入命令错误，请重新输入" << endl;
			}

		}

		else if (menu.compare("help") == 0) { //help菜单
			printf(
				"select <items> where a=?           ------------ 查询格式\n"\
				"insert a c1 c2 c3 c4               ------------ 插入数据格式\n "\
				"reset                              ------------ 重置数据库\n"\
				"shutdown                           ------------ 停机重启\n"\
				"select <items> where a between ? and ?      --- 查询小范围结果\n"\
				"select into xxx.csv where a between ? and ?  -- 数据导出\n"
			);
		}


		else if (menu.compare("reset") == 0) {
			remove("dataRecord.ibd");
			cout << "数据库已重置~" << endl;

			createDataFile();
		}

		/* 关闭程序 */
		else if (menu.compare("shutdown") == 0) {
			return 0;
		}

		else {
			cout << "输入命令错误，请重新输入" << endl;
		}
	}
}
