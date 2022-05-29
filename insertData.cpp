#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <fstream>

extern FILE* fo; // ȫ�ֱ��� fo

/** �궨�� **/
#define Page_size 4*1024 // ҳ��С��Ϊ4KB
#define Index_page_size 500 // ����ҳ�����500��key
#define Kid_page_size 7 // Ҷ��ҳ�����7��key

using namespace std;

/* �ļ�ͷ�ṹ�� */
typedef struct {
	int steps; // B+���Ľ���
	unsigned int pageSize; // ҳ��С
	unsigned int pageCount; // �Ѵ�ҳ������
	unsigned int rootPagePla; // b+����ҳ������λ�ã�����ֵ��������ļ�ͷ����ƫ��ֵ��
}File_head;

/* ����ҳ�ṹ�� */
typedef struct {
	int pageType; // ҳ�����ͣ������ж�������ҳ����������ҳ
	int keyCount; // �Ѵ�key������
	unsigned int parentPagePla; // ��ҳ������λ��
	unsigned int key[Index_page_size + 1]; // ����ֵ
	unsigned int kidPagePla[Index_page_size + 1]; // ��ҳ������λ��
}Index_page;

/* ���ݽṹ�� */
typedef struct {
	char c1[129];
	char c2[129];
	char c3[129];
	char c4[81];
}Data;

/* ����ҳ�ṹ�� */
typedef struct {
	int pageType; // ҳ�����ͣ������ж�������ҳ����������ҳ
	int keyCount; // �Ѵ�key������
	bool isIncInsert; // �Ƿ��������
	bool isDecInsert; // �Ƿ�ݼ�����
	unsigned int parentPagePla; // ��ҳ������λ��
	unsigned int prevPagePla; // ǰһ��ҳ������λ��
	unsigned int nextPagePla; // ��һ��ҳ������λ��
	unsigned int key[Kid_page_size + 1]; // һ�����ݵ�keyֵ
	Data data[Kid_page_size + 1]; // һ�����ݵ�dataֵ
}Data_page;

/* ���ҽ���ṹ�� */
typedef struct {
	unsigned int pla; // ָ���λ��
	int index; // ��ҳ�еĹؼ������
	bool isFound; // �Ƿ��ҵ�
} Result;


/* �ӿ� */
bool insertData(Data data, unsigned int key); // ���뵥������
int loadCsvData(string csv_file_name); // ���ļ�����ķ�ʽ�����������
int searchBTIndex(Index_page* btPageBuf, unsigned int key); // ����key������ҳ�е�λ��
void searchDataPage(unsigned int page__pos, unsigned int key, Result* result); // ��������ҳ
bool selectData(unsigned int key, vector<bool> field); // ������ֵ��ѯ����
void selectRangeData(unsigned int left, unsigned int right, vector<bool> field); // ����������Χɨ���ѯ����
int outputCsvData(unsigned int left, unsigned int right, string csv_file_name); // ����������Χɨ��Ĳ�ѯ��������

/* ��ȡ�ļ���ҳ������ */
unsigned int getPageCount() {
	// ���벢��ʼ��ҳ
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head)); memset(fileHeadBuf, 0, sizeof(File_head));
	// ָ��λ�ÿ�ʼ��
	fseek(fo, 0, SEEK_SET);
	fread(fileHeadBuf, sizeof(File_head), 1, fo);
	unsigned int pageCount = fileHeadBuf->pageCount;
	// �ͷ�ҳ
	free(fileHeadBuf);
	return pageCount;
}

/* �����ļ�ͷ��Ϣ */
void updateFileHead(unsigned int new_root_page_pos) {
	// ���벢��ʼ��ҳ
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head)); memset(fileHeadBuf, 0, sizeof(File_head));
	// ָ��λ�ÿ�ʼ��
	fseek(fo, 0, SEEK_SET);
	fread(fileHeadBuf, sizeof(File_head), 1, fo);
	if (new_root_page_pos > 0) {
		fileHeadBuf->rootPagePla = new_root_page_pos;
	}
	fileHeadBuf->pageCount++;
	// ָ��λ�ÿ�ʼд
	fseek(fo, 0, SEEK_SET);
	fwrite(fileHeadBuf, sizeof(File_head), 1, fo);
	// �ͷ�ҳ
	free(fileHeadBuf);
}

/* ��������ҳ */
Index_page insertBTPage(unsigned int page_pos, int index, unsigned int key, unsigned int ap) {

	Index_page buf;
	Index_page* btPageBuf = &buf;

	// ָ��λ�ÿ�ʼ��
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
	// ָ��λ�ÿ�ʼд
	fseek(fo, page_pos, SEEK_SET); fwrite(btPageBuf, sizeof(Index_page), 1, fo);
	return (*btPageBuf);
}

/* ��������ҳ */
Data_page insertDataPage(unsigned int page_pos, int index, unsigned int key, Data data) {
	Data_page buf;
	Data_page* dataPageBuf = &buf;

	// ָ��λ�ÿ�ʼ��
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
	// ָ��λ�ÿ�ʼд
	fseek(fo, page_pos, SEEK_SET); fwrite(dataPageBuf, sizeof(Data_page), 1, fo);
	return (*dataPageBuf);
}

/* ��������ҳ */
unsigned int splitDataPage(Data_page* dataPageBuf, unsigned int page_pos, int s) {

	int i, j;
	int n = dataPageBuf->keyCount;
	unsigned int ap;

	// ���벢��ʼ��ҳ
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
	// �ļ�ĩβ��ʼд
	fseek(fo, pageCount * Page_size, SEEK_SET); ap = ftell(fo); fwrite(apDataPageBuf, sizeof(Data_page), 1, fo);

	updateFileHead(0);

	dataPageBuf->keyCount = s + 1;
	dataPageBuf->nextPagePla = ap;
	// ָ��λ�ÿ�ʼд
	fseek(fo, page_pos, SEEK_SET); fwrite(dataPageBuf, sizeof(Data_page), 1, fo);
	// �ͷ�ҳ
	free(apDataPageBuf);
	return ap;
}

/* ��������ҳ */
unsigned int splitBTPage(Index_page* btPageBuf, unsigned int page_pos, int s) {

	int i, j;
	int n = btPageBuf->keyCount;
	unsigned int ap;

	// ���벢��ʼ��ҳ
	Index_page* apBTPageBuf = (Index_page*)malloc(sizeof(Index_page)); memset(apBTPageBuf, 0, sizeof(Index_page));
	// ���벢��ʼ��ҳ
	Index_page* apBTPageBufChild = (Index_page*)malloc(sizeof(Index_page)); memset(apBTPageBufChild, 0, sizeof(Index_page));
	// ���벢��ʼ��ҳ
	Data_page* apDataPageBufChild = (Data_page*)malloc(sizeof(Data_page)); memset(apDataPageBufChild, 0, sizeof(Data_page));


	for (i = s, j = 0; i < n; i++, j++) {
		apBTPageBuf->key[j] = btPageBuf->key[i];
		apBTPageBuf->kidPagePla[j] = btPageBuf->kidPagePla[i];
	}
	apBTPageBuf->pageType = 1;
	apBTPageBuf->keyCount = n - s;
	apBTPageBuf->parentPagePla = btPageBuf->parentPagePla;

	unsigned int pageCount = getPageCount();
	// �ļ�ĩβ��ʼд
	fseek(fo, pageCount * Page_size, SEEK_SET);  ap = ftell(fo); fwrite(apBTPageBuf, sizeof(Index_page), 1, fo);


	updateFileHead(0);

	for (int i = 0; i < n - s; i++) {
		if (apBTPageBuf->kidPagePla[i] != 0) {
			// ָ��λ�ÿ�ʼ��
			fseek(fo, apBTPageBuf->kidPagePla[i], SEEK_SET);
			fread(apBTPageBufChild, sizeof(Index_page), 1, fo);
			if (apBTPageBufChild->pageType == 2) {
				// ָ��λ�ÿ�ʼ��
				fseek(fo, apBTPageBuf->kidPagePla[i], SEEK_SET);
				fread(apDataPageBufChild, sizeof(Data_page), 1, fo);
				apDataPageBufChild->parentPagePla = ap;
				// ָ��λ�ÿ�ʼд
				fseek(fo, apBTPageBuf->kidPagePla[i], SEEK_SET); fwrite(apDataPageBufChild, sizeof(Data_page), 1, fo);
			}
			else {
				apBTPageBufChild->parentPagePla = ap;
				// ָ��λ�ÿ�ʼд
				fseek(fo, apBTPageBuf->kidPagePla[i], SEEK_SET); fwrite(apBTPageBufChild, sizeof(Index_page), 1, fo);
			}
		}
	}

	btPageBuf->keyCount = s;

	// ָ��λ�ÿ�ʼд
	fseek(fo, page_pos, SEEK_SET); fwrite(btPageBuf, sizeof(Index_page), 1, fo);
	// �ͷ�ҳ
	free(apBTPageBuf);
	// �ͷ�ҳ
	free(apBTPageBufChild);
	// �ͷ�ҳ
	free(apDataPageBufChild);
	return (ap);
}

/* �½���ҳ�� */
void newRoot(unsigned int rootPagePla, unsigned int key, unsigned int ap) {
	unsigned int pla;
	// ���벢��ʼ��ҳ
	Index_page* rootBTPageBuf = (Index_page*)malloc(sizeof(Index_page)); memset(rootBTPageBuf, 0, sizeof(Index_page));
	rootBTPageBuf->pageType = 1;
	rootBTPageBuf->keyCount = 2;
	rootBTPageBuf->kidPagePla[0] = rootPagePla;
	rootBTPageBuf->kidPagePla[1] = ap;
	rootBTPageBuf->key[0] = 0;
	rootBTPageBuf->key[1] = key;

	unsigned int pageCount = getPageCount();
	// �ļ�ĩβ��ʼд
	fseek(fo, pageCount * Page_size, SEEK_SET); pla = ftell(fo); fwrite(rootBTPageBuf, sizeof(Index_page), 1, fo);

	/* ��ȡԭ��ҳ�����parentPagePlaλ�� */
	// ָ��λ�ÿ�ʼ��
	fseek(fo, rootPagePla, SEEK_SET);
	fread(rootBTPageBuf, sizeof(Index_page), 1, fo);
	rootBTPageBuf->parentPagePla = pla;
	// ָ��λ�ÿ�ʼд
	fseek(fo, rootPagePla, SEEK_SET); fwrite(rootBTPageBuf, sizeof(Index_page), 1, fo);

	/* ��ȡ����ҳ�����parentPagePlaλ�� */

	// ָ��λ�ÿ�ʼ��
	fseek(fo, ap, SEEK_SET);
	fread(rootBTPageBuf, sizeof(Index_page), 1, fo);
	rootBTPageBuf->parentPagePla = pla;
	// ָ��λ�ÿ�ʼд
	fseek(fo, ap, SEEK_SET); fwrite(rootBTPageBuf, sizeof(Index_page), 1, fo);

	updateFileHead(pla);

	// �ͷ�ҳ
	free(rootBTPageBuf);
}

/* �����ݲ���B+�� */
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
		// ָ��λ�ÿ�ʼ��
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
				// ָ��λ�ÿ�ʼ��
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
/* ���뵥������ */
bool insertData(Data data, unsigned int key) {
	// ���벢��ʼ��ҳ
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head)); memset(fileHeadBuf, 0, sizeof(File_head));


	// �� rb+ ��ʽ���ļ�,�������ļ�ͷҳ��
	fo = fopen("dataRecord.ibd", "rb+"); fread(fileHeadBuf, sizeof(File_head), 1, fo);
	Result result;
	searchDataPage(fileHeadBuf->rootPagePla, key, &result);
	if (result.isFound) {
		cout << "index:" << key << " already exists" << endl;
		// �ͷ�ҳ
		free(fileHeadBuf);
		// �ر��ļ�
		fclose(fo);
		return false;
	}
	else {
		insertBPlusTree(fileHeadBuf->rootPagePla, key, result.pla, result.index, data);
		// �ͷ�ҳ
		free(fileHeadBuf);
		// �ر��ļ�
		fclose(fo);
		return true;
	}
}

/* ���ļ�����ķ�ʽ����������� */
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