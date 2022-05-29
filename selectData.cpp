#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <fstream>

extern FILE* fo; // 全局变量 fo

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

/* 查找key在索引页中的位置 */
int searchBTIndex(Index_page* btPageBuf, unsigned int key) {
	int left = 0;
	int right = btPageBuf->keyCount - 1;
	while (left <= right) {
		int mid = left + (right - left) / 2;
		if (btPageBuf->key[mid] == key) return mid;
		else if (btPageBuf->key[mid] < key) left = mid + 1;
		else right = mid - 1;
	}
	return left;
}

/* 查找key在数据页中的位置 */
int searchDataIndex(Data_page* dataPageBuf, unsigned int key) {
	int left = 0;
	int right = dataPageBuf->keyCount - 1;
	while (left <= right) {
		int mid = left + (right - left) / 2;
		if (dataPageBuf->key[mid] == key) return mid;
		else if (dataPageBuf->key[mid] < key) left = mid + 1;
		else right = mid - 1;
	}
	return left;
}

/* 打印字段信息 */
void printHeader(vector<bool> field) {
	string set[5] = { "a","c1","c2","c3","c4" };
	bool has = false;
	for (int i = 0; i < 5; i++) {
		if (field[i]) {
			if (!has) {
				cout << '|';
			}
			if (i != 0) cout << ' ';
			cout << set[i] << '|';
			has = true;
		}
	}
	cout << endl;
}

/* 打印数据信息 */
void printData(Data_page* dataPageBuf, vector<bool> field, int index) {
	bool has = false;
	for (int i = 0; i < 5; i++) {
		if (field[i]) {
			if (!has) cout << '|';
			if (i == 0)
				cout << dataPageBuf->key[index] << '|';
			else if (i == 1)
				cout << ' ' << dataPageBuf->data[index].c1 << '|';
			else if (i == 2)
				cout << ' ' << dataPageBuf->data[index].c2 << '|';
			else if (i == 3)
				cout << ' ' << dataPageBuf->data[index].c3 << '|';
			else
				cout << ' ' << dataPageBuf->data[index].c4 << '|';
			has = true;
		}
	}
	cout << endl;
}

// 查找数据页
void searchDataPage(unsigned int page__pos, unsigned int key, Result* result) {

	int i = 0;
	bool isFound = false;

	// 申请并初始化页 MALLOC_PAGE(btPageBuf, Index_page);
	Index_page* btPageBuf = (Index_page*)malloc(sizeof(Index_page));
	memset(btPageBuf, 0, sizeof(Index_page));

	// 申请并初始化页 MALLOC_PAGE(dataPageBuf, Data_page);
	Data_page* dataPageBuf = (Data_page*)malloc(sizeof(Data_page));
	memset(dataPageBuf, 0, sizeof(Data_page));

	while (page__pos != 0 && !isFound) {
		// 指定位置开始读 FSEEK_FIXED_READ(fo, page__pos, btPageBuf, sizeof(Index_page));
		fseek(fo, page__pos, SEEK_SET);
		fread(btPageBuf, sizeof(Index_page), 1, fo);

		if (btPageBuf->pageType == 2) {
			// 指定位置开始读 FSEEK_FIXED_READ(fo, page__pos, dataPageBuf, sizeof(Data_page));
			fseek(fo, page__pos, SEEK_SET);
			fread(dataPageBuf, sizeof(Data_page), 1, fo);

			i = searchDataIndex(dataPageBuf, key);
			if (i < dataPageBuf->keyCount && dataPageBuf->key[i] == key)
				isFound = true;
			break;
		}
		else {
			i = searchBTIndex(btPageBuf, key);
			page__pos = btPageBuf->kidPagePla[i - 1];
		}

	}
	result->pla = page__pos;
	result->index = i;
	result->isFound = isFound;

	// 释放页FREE_PAGE(btPageBuf);
	free(btPageBuf);
	// 释放页FREE_PAGE(dataPageBuf);
	free(dataPageBuf);
}

/* 主键等值查询数据 */
bool selectData(unsigned int key, vector<bool> field) {

	// 申请并初始化页 MALLOC_PAGE(fileHeadBuf, File_head);
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head));
	memset(fileHeadBuf, 0, sizeof(File_head));

	// 申请并初始化页 MALLOC_PAGE(dataPageBuf, Data_page);
	Data_page* dataPageBuf = (Data_page*)malloc(sizeof(Data_page));
	memset(dataPageBuf, 0, sizeof(Data_page));

	// 以mode形式打开文件,并读出文件头页面 OPEN_FILE_READ("dataRecord.ibd", "rb+", fileHeadBuf, sizeof(File_head));
	// 以mode形式打开文件 fo = OPEN_FILE("dataRecord.ibd", "rb+");
	fopen("dataRecord.ibd", "rb+");
	fread(fileHeadBuf, sizeof(File_head), 1, fo);

	Result result;
	searchDataPage(fileHeadBuf->rootPagePla, key, &result);
	if (!result.isFound) {
		// 释放页	FREE_PAGE(fileHeadBuf);
		free(fileHeadBuf);
		// 释放页	FREE_PAGE(dataPageBuf);
		free(dataPageBuf);
		// 关闭文件	CLOSE_FILE(fo);
		fclose(fo);
		return false;
	}
	else {
	// 指定位置开始读	FSEEK_FIXED_READ(fo, result.pla, dataPageBuf, sizeof(Data_page));
		fseek(fo, result.pla, SEEK_SET);
		fread(dataPageBuf, sizeof(Data_page), 1, fo);

		printHeader(field);
		printData(dataPageBuf, field, result.index);
		// 释放页 FREE_PAGE(fileHeadBuf);
		free(fileHeadBuf);
		// 释放页 FREE_PAGE(dataPageBuf);
		free(dataPageBuf);
		// 关闭文件 CLOSE_FILE(fo);
		fclose(fo);

		return true;
	}
}

/* 主键索引范围扫描查询数据 */
void selectRangeData(unsigned int left, unsigned int right, vector<bool> field) {

	// 申请并初始化页 MALLOC_PAGE(fileHeadBuf, File_head);
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head));
	memset(fileHeadBuf, 0, sizeof(File_head));
	// 申请并初始化页 MALLOC_PAGE(dataPageBuf, Data_page);
	Data_page* dataPageBuf = (Data_page*)malloc(sizeof(Data_page));
	memset(dataPageBuf, 0, sizeof(Data_page));

	// 以mode形式打开文件,并读出文件头页面 OPEN_FILE_READ("dataRecord.ibd", "rb+", fileHeadBuf, sizeof(File_head));
	// 以mode形式打开文件 fo = OPEN_FILE("dataRecord.ibd", "rb+");
	fopen("dataRecord.ibd", "rb+");
	fread(fileHeadBuf, sizeof(File_head), 1, fo);

	Result result;
	searchDataPage(fileHeadBuf->rootPagePla, left, &result);

	// 指定位置开始读 FSEEK_FIXED_READ(fo, result.pla, dataPageBuf, sizeof(Data_page));
	fseek(fo, result.pla, SEEK_SET);
	fread(dataPageBuf, sizeof(Data_page), 1, fo);

	printHeader(field);
	unsigned int cur = result.index;
	while (dataPageBuf->key[cur] != 0 && dataPageBuf->key[cur] <= right) {

		printData(dataPageBuf, field, cur);
		if (cur < dataPageBuf->keyCount - 1) {
			cur++;
		}
		else {
			if (dataPageBuf->nextPagePla != 0) {
				// 指定位置开始读 FSEEK_FIXED_READ(fo, dataPageBuf->nextPagePla, dataPageBuf, sizeof(Data_page));
				fseek(fo, dataPageBuf->nextPagePla, SEEK_SET);
				fread(dataPageBuf, sizeof(Data_page), 1, fo);

				cur = 0;
			}
			else {
				break;
			}

		}
	}
	// 释放页 FREE_PAGE(fileHeadBuf);
	free(fileHeadBuf);
	// 释放页 FREE_PAGE(dataPageBuf);
	free(dataPageBuf);
	// 关闭文件 CLOSE_FILE(fo);
	fclose(fo);
}

/* 主键索引范围扫描的查询导出数据 */
int outputCsvData(unsigned int left, unsigned int right, string csv_file_name) {
	ofstream outFile(csv_file_name, ios::out);
	// 申请并初始化页 MALLOC_PAGE(fileHeadBuf, File_head);
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head));
	memset(fileHeadBuf, 0, sizeof(File_head));

	// 申请并初始化页MALLOC_PAGE(dataPageBuf, Data_page);
	Data_page* dataPageBuf = (Data_page*)malloc(sizeof(Data_page));
	memset(dataPageBuf, 0, sizeof(Data_page));

	// 以mode形式打开文件,并读出文件头页面 OPEN_FILE_READ("dataRecord.ibd", "rb+", fileHeadBuf, sizeof(File_head));
	// 以mode形式打开文件 fo = OPEN_FILE("dataRecord.ibd", "rb+");
	fopen("dataRecord.ibd", "rb+");

	fread(fileHeadBuf, sizeof(File_head), 1, fo);

	Result result;
	searchDataPage(fileHeadBuf->rootPagePla, left, &result);

	// 指定位置开始读 FSEEK_FIXED_READ(fo, result.pla, dataPageBuf, sizeof(Data_page));
	fseek(fo, result.pla, SEEK_SET);
	fread(dataPageBuf, sizeof(Data_page), 1, fo);

	int cnt = 0;

	unsigned int cur = result.index;
	while (dataPageBuf->key[cur] <= right) {

		outFile << dataPageBuf->key[cur];
		outFile << (char)0x09 << dataPageBuf->data[cur].c1;
		outFile << (char)0x09 << dataPageBuf->data[cur].c2;
		outFile << (char)0x09 << dataPageBuf->data[cur].c3;
		outFile << (char)0x09 << dataPageBuf->data[cur].c4;
		outFile << endl;

		cnt++;

		if (cur < dataPageBuf->keyCount - 1) {
			cur++;
		}
		else {
			if (dataPageBuf->nextPagePla != 0) {
				// 指定位置开始读 FSEEK_FIXED_READ(fo, dataPageBuf->nextPagePla, dataPageBuf, sizeof(Data_page));
				fseek(fo, dataPageBuf->nextPagePla, SEEK_SET);
				fread(dataPageBuf, sizeof(Data_page), 1, fo);
				cur = 0;
			}
			else {
				break;
			}
		}
	}
	outFile.close();
	// 释放页 FREE_PAGE(fileHeadBuf);
	free(fileHeadBuf);
	// 释放页 FREE_PAGE(dataPageBuf);
	free(dataPageBuf);
	// 关闭文件 CLOSE_FILE(fo);
	fclose(fo);
	return cnt;
}