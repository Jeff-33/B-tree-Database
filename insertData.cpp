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

/* 获取文件中页的数量 */
unsigned int getPageCount() {
	// 申请并初始化页
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head)); memset(fileHeadBuf, 0, sizeof(File_head));
	// 指定位置开始读
	fseek(fo, 0, SEEK_SET);
	fread(fileHeadBuf, sizeof(File_head), 1, fo);
	unsigned int pageCount = fileHeadBuf->pageCount;
	// 释放页
	free(fileHeadBuf);
	return pageCount;
}

/* 更新文件头信息 */
void updateFileHead(unsigned int new_root_page_pos) {
	// 申请并初始化页
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head)); memset(fileHeadBuf, 0, sizeof(File_head));
	// 指定位置开始读
	fseek(fo, 0, SEEK_SET);
	fread(fileHeadBuf, sizeof(File_head), 1, fo);
	if (new_root_page_pos > 0) {
		fileHeadBuf->rootPagePla = new_root_page_pos;
	}
	fileHeadBuf->pageCount++;
	// 指定位置开始写
	fseek(fo, 0, SEEK_SET);
	fwrite(fileHeadBuf, sizeof(File_head), 1, fo);
	// 释放页
	free(fileHeadBuf);
}

/* 插入索引页 */
Index_page insertBTPage(unsigned int page_pos, int index, unsigned int key, unsigned int ap) {

	Index_page buf;
	Index_page* btPageBuf = &buf;

	// 指定位置开始读
	fseek(fo, page_pos, SEEK_SET);
	fread(btPageBuf, sizeof(Index_page), 1, fo);
	int j;
	for (j = btPageBuf->keyCount - 1; j >= index; j--) {
		btPageBuf->key[j + 1] = btPageBuf->key[j];
		btPageBuf->kidPagePla[j + 1] = btPageBuf->kidPagePla[j];
	}
	btPageBuf->key[index] = key;
	btPageBuf->kidPagePla[index] = ap;

	btPageBuf->keyCount++;
	// 指定位置开始写
	fseek(fo, page_pos, SEEK_SET); fwrite(btPageBuf, sizeof(Index_page), 1, fo);
	return (*btPageBuf);
}

/* 插入数据页 */
Data_page insertDataPage(unsigned int page_pos, int index, unsigned int key, Data data) {
	Data_page buf;
	Data_page* dataPageBuf = &buf;

	// 指定位置开始读
	fseek(fo, page_pos, SEEK_SET);
	fread(dataPageBuf, sizeof(Data_page), 1, fo);
	if (index != 0) dataPageBuf->isDecInsert = false;
	if (index != dataPageBuf->keyCount) dataPageBuf->isIncInsert = false;

	int j;
	if (dataPageBuf->keyCount != 0) {
		for (j = dataPageBuf->keyCount - 1; j >= index; j--) {
			dataPageBuf->key[j + 1] = dataPageBuf->key[j];
			dataPageBuf->data[j + 1] = dataPageBuf->data[j];
		}
	}

	dataPageBuf->key[index] = key;
	dataPageBuf->data[index] = data;

	dataPageBuf->keyCount++;
	// 指定位置开始写
	fseek(fo, page_pos, SEEK_SET); fwrite(dataPageBuf, sizeof(Data_page), 1, fo);
	return (*dataPageBuf);
}

/* 分裂数据页 */
unsigned int splitDataPage(Data_page* dataPageBuf, unsigned int page_pos, int s) {

	int i, j;
	int n = dataPageBuf->keyCount;
	unsigned int ap;

	// 申请并初始化页
	Data_page* apDataPageBuf = (Data_page*)malloc(sizeof(Data_page)); memset(apDataPageBuf, 0, sizeof(Data_page));

	for (i = s + 1, j = 0; i < n; i++, j++) {
		apDataPageBuf->key[j] = dataPageBuf->key[i];
		apDataPageBuf->data[j] = dataPageBuf->data[i];
	}
	apDataPageBuf->pageType = 2;
	apDataPageBuf->keyCount = n - s - 1;
	apDataPageBuf->isIncInsert = true;
	apDataPageBuf->isDecInsert = true;
	apDataPageBuf->parentPagePla = dataPageBuf->parentPagePla;
	apDataPageBuf->prevPagePla = page_pos;
	apDataPageBuf->nextPagePla = dataPageBuf->nextPagePla;

	unsigned int pageCount = getPageCount();
	// 文件末尾开始写
	fseek(fo, pageCount * Page_size, SEEK_SET); ap = ftell(fo); fwrite(apDataPageBuf, sizeof(Data_page), 1, fo);

	updateFileHead(0);

	dataPageBuf->keyCount = s + 1;
	dataPageBuf->nextPagePla = ap;
	// 指定位置开始写
	fseek(fo, page_pos, SEEK_SET); fwrite(dataPageBuf, sizeof(Data_page), 1, fo);
	// 释放页
	free(apDataPageBuf);
	return ap;
}

/* 分裂索引页 */
unsigned int splitBTPage(Index_page* btPageBuf, unsigned int page_pos, int s) {

	int i, j;
	int n = btPageBuf->keyCount;
	unsigned int ap;

	// 申请并初始化页
	Index_page* apBTPageBuf = (Index_page*)malloc(sizeof(Index_page)); memset(apBTPageBuf, 0, sizeof(Index_page));
	// 申请并初始化页
	Index_page* apBTPageBufChild = (Index_page*)malloc(sizeof(Index_page)); memset(apBTPageBufChild, 0, sizeof(Index_page));
	// 申请并初始化页
	Data_page* apDataPageBufChild = (Data_page*)malloc(sizeof(Data_page)); memset(apDataPageBufChild, 0, sizeof(Data_page));


	for (i = s, j = 0; i < n; i++, j++) {
		apBTPageBuf->key[j] = btPageBuf->key[i];
		apBTPageBuf->kidPagePla[j] = btPageBuf->kidPagePla[i];
	}
	apBTPageBuf->pageType = 1;
	apBTPageBuf->keyCount = n - s;
	apBTPageBuf->parentPagePla = btPageBuf->parentPagePla;

	unsigned int pageCount = getPageCount();
	// 文件末尾开始写
	fseek(fo, pageCount * Page_size, SEEK_SET);  ap = ftell(fo); fwrite(apBTPageBuf, sizeof(Index_page), 1, fo);


	updateFileHead(0);

	for (int i = 0; i < n - s; i++) {
		if (apBTPageBuf->kidPagePla[i] != 0) {
			// 指定位置开始读
			fseek(fo, apBTPageBuf->kidPagePla[i], SEEK_SET);
			fread(apBTPageBufChild, sizeof(Index_page), 1, fo);
			if (apBTPageBufChild->pageType == 2) {
				// 指定位置开始读
				fseek(fo, apBTPageBuf->kidPagePla[i], SEEK_SET);
				fread(apDataPageBufChild, sizeof(Data_page), 1, fo);
				apDataPageBufChild->parentPagePla = ap;
				// 指定位置开始写
				fseek(fo, apBTPageBuf->kidPagePla[i], SEEK_SET); fwrite(apDataPageBufChild, sizeof(Data_page), 1, fo);
			}
			else {
				apBTPageBufChild->parentPagePla = ap;
				// 指定位置开始写
				fseek(fo, apBTPageBuf->kidPagePla[i], SEEK_SET); fwrite(apBTPageBufChild, sizeof(Index_page), 1, fo);
			}
		}
	}

	btPageBuf->keyCount = s;

	// 指定位置开始写
	fseek(fo, page_pos, SEEK_SET); fwrite(btPageBuf, sizeof(Index_page), 1, fo);
	// 释放页
	free(apBTPageBuf);
	// 释放页
	free(apBTPageBufChild);
	// 释放页
	free(apDataPageBufChild);
	return (ap);
}

/* 新建根页面 */
void newRoot(unsigned int rootPagePla, unsigned int key, unsigned int ap) {
	unsigned int pla;
	// 申请并初始化页
	Index_page* rootBTPageBuf = (Index_page*)malloc(sizeof(Index_page)); memset(rootBTPageBuf, 0, sizeof(Index_page));
	rootBTPageBuf->pageType = 1;
	rootBTPageBuf->keyCount = 2;
	rootBTPageBuf->kidPagePla[0] = rootPagePla;
	rootBTPageBuf->kidPagePla[1] = ap;
	rootBTPageBuf->key[0] = 0;
	rootBTPageBuf->key[1] = key;

	unsigned int pageCount = getPageCount();
	// 文件末尾开始写
	fseek(fo, pageCount * Page_size, SEEK_SET); pla = ftell(fo); fwrite(rootBTPageBuf, sizeof(Index_page), 1, fo);

	/* 读取原根页面更新parentPagePla位置 */
	// 指定位置开始读
	fseek(fo, rootPagePla, SEEK_SET);
	fread(rootBTPageBuf, sizeof(Index_page), 1, fo);
	rootBTPageBuf->parentPagePla = pla;
	// 指定位置开始写
	fseek(fo, rootPagePla, SEEK_SET); fwrite(rootBTPageBuf, sizeof(Index_page), 1, fo);

	/* 读取分裂页面更新parentPagePla位置 */

	// 指定位置开始读
	fseek(fo, ap, SEEK_SET);
	fread(rootBTPageBuf, sizeof(Index_page), 1, fo);
	rootBTPageBuf->parentPagePla = pla;
	// 指定位置开始写
	fseek(fo, ap, SEEK_SET); fwrite(rootBTPageBuf, sizeof(Index_page), 1, fo);

	updateFileHead(pla);

	// 释放页
	free(rootBTPageBuf);
}

/* 将数据插入B+树 */
void insertBPlusTree(unsigned int head_pos, unsigned int key, unsigned int page_pos, int i, Data data) {
	int s = 0;
	unsigned int ap = 0;
	bool finished = false;
	bool needNewRoot = false;
	Data_page dataPageBuf;
	dataPageBuf = insertDataPage(page_pos, i, key, data);
	if (dataPageBuf.keyCount <= Kid_page_size) finished = true;

	if (!finished) {
		if (dataPageBuf.isIncInsert) {
			s = Kid_page_size - 1;
		}
		else if (dataPageBuf.isDecInsert) {
			s = 0;
		}
		else {
			s = (Kid_page_size + 1) / 2 - 1;
		}
		ap = splitDataPage(&dataPageBuf, page_pos, s);
		key = dataPageBuf.key[s];
		page_pos = dataPageBuf.parentPagePla;
		Index_page btPageBuf;
		// 指定位置开始读
		fseek(fo, page_pos, SEEK_SET);
		fread(&btPageBuf, sizeof(Index_page), 1, fo);
		i = searchBTIndex(&btPageBuf, key);
	}

	Index_page btPageBuf;
	while (!needNewRoot && !finished) {
		btPageBuf = insertBTPage(page_pos, i, key, ap);
		if (btPageBuf.keyCount <= Index_page_size) {
			finished = true;
		}
		else {
			s = (Index_page_size + 1) / 2 - 1;
			ap = splitBTPage(&btPageBuf, page_pos, s);
			key = btPageBuf.key[s];
			if (btPageBuf.parentPagePla != 0) {
				page_pos = btPageBuf.parentPagePla;
				// 指定位置开始读
				fseek(fo, page_pos, SEEK_SET);
				fread(&btPageBuf, sizeof(Index_page), 1, fo);
				i = searchBTIndex(&btPageBuf, key);
			}
			else {
				needNewRoot = true;
			}
		}
	}
	if (needNewRoot) {
		newRoot(head_pos, key, ap);
	}
}
/* 插入单条数据 */
bool insertData(Data data, unsigned int key) {
	// 申请并初始化页
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head)); memset(fileHeadBuf, 0, sizeof(File_head));


	// 以 rb+ 形式打开文件,并读出文件头页面
	fo = fopen("dataRecord.ibd", "rb+"); fread(fileHeadBuf, sizeof(File_head), 1, fo);
	Result result;
	searchDataPage(fileHeadBuf->rootPagePla, key, &result);
	if (result.isFound) {
		cout << "index:" << key << " already exists" << endl;
		// 释放页
		free(fileHeadBuf);
		// 关闭文件
		fclose(fo);
		return false;
	}
	else {
		insertBPlusTree(fileHeadBuf->rootPagePla, key, result.pla, result.index, data);
		// 释放页
		free(fileHeadBuf);
		// 关闭文件
		fclose(fo);
		return true;
	}
}

/* 以文件导入的方式插入大量数据 */
int loadCsvData(string csv_file_name) {
	ifstream inFile(csv_file_name, ios::in);
	string lineStr;
	int cnt = 0;
	Data data;
	while (getline(inFile, lineStr))
	{
		stringstream ss(lineStr);
		string str;
		vector<string> strList;
		while (getline(ss, str, (char)0x09))
			strList.push_back(str);
		unsigned int key = stoi(strList[0]);
		strcpy(data.c1, strList[1].c_str());
		strcpy(data.c2, strList[2].c_str());
		strcpy(data.c3, strList[3].c_str());
		strcpy(data.c4, strList[4].c_str());
		if (insertData(data, key))
			cnt++;
	}

	inFile.close();
	return cnt;
}