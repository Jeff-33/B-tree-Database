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

/* ����key������ҳ�е�λ�� */
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

/* ����key������ҳ�е�λ�� */
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

/* ��ӡ�ֶ���Ϣ */
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

/* ��ӡ������Ϣ */
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

// ��������ҳ
void searchDataPage(unsigned int page__pos, unsigned int key, Result* result) {

	int i = 0;
	bool isFound = false;

	// ���벢��ʼ��ҳ MALLOC_PAGE(btPageBuf, Index_page);
	Index_page* btPageBuf = (Index_page*)malloc(sizeof(Index_page));
	memset(btPageBuf, 0, sizeof(Index_page));

	// ���벢��ʼ��ҳ MALLOC_PAGE(dataPageBuf, Data_page);
	Data_page* dataPageBuf = (Data_page*)malloc(sizeof(Data_page));
	memset(dataPageBuf, 0, sizeof(Data_page));

	while (page__pos != 0 && !isFound) {
		// ָ��λ�ÿ�ʼ�� FSEEK_FIXED_READ(fo, page__pos, btPageBuf, sizeof(Index_page));
		fseek(fo, page__pos, SEEK_SET);
		fread(btPageBuf, sizeof(Index_page), 1, fo);

		if (btPageBuf->pageType == 2) {
			// ָ��λ�ÿ�ʼ�� FSEEK_FIXED_READ(fo, page__pos, dataPageBuf, sizeof(Data_page));
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

	// �ͷ�ҳFREE_PAGE(btPageBuf);
	free(btPageBuf);
	// �ͷ�ҳFREE_PAGE(dataPageBuf);
	free(dataPageBuf);
}

/* ������ֵ��ѯ���� */
bool selectData(unsigned int key, vector<bool> field) {

	// ���벢��ʼ��ҳ MALLOC_PAGE(fileHeadBuf, File_head);
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head));
	memset(fileHeadBuf, 0, sizeof(File_head));

	// ���벢��ʼ��ҳ MALLOC_PAGE(dataPageBuf, Data_page);
	Data_page* dataPageBuf = (Data_page*)malloc(sizeof(Data_page));
	memset(dataPageBuf, 0, sizeof(Data_page));

	// ��mode��ʽ���ļ�,�������ļ�ͷҳ�� OPEN_FILE_READ("dataRecord.ibd", "rb+", fileHeadBuf, sizeof(File_head));
	// ��mode��ʽ���ļ� fo = OPEN_FILE("dataRecord.ibd", "rb+");
	fopen("dataRecord.ibd", "rb+");
	fread(fileHeadBuf, sizeof(File_head), 1, fo);

	Result result;
	searchDataPage(fileHeadBuf->rootPagePla, key, &result);
	if (!result.isFound) {
		// �ͷ�ҳ	FREE_PAGE(fileHeadBuf);
		free(fileHeadBuf);
		// �ͷ�ҳ	FREE_PAGE(dataPageBuf);
		free(dataPageBuf);
		// �ر��ļ�	CLOSE_FILE(fo);
		fclose(fo);
		return false;
	}
	else {
	// ָ��λ�ÿ�ʼ��	FSEEK_FIXED_READ(fo, result.pla, dataPageBuf, sizeof(Data_page));
		fseek(fo, result.pla, SEEK_SET);
		fread(dataPageBuf, sizeof(Data_page), 1, fo);

		printHeader(field);
		printData(dataPageBuf, field, result.index);
		// �ͷ�ҳ FREE_PAGE(fileHeadBuf);
		free(fileHeadBuf);
		// �ͷ�ҳ FREE_PAGE(dataPageBuf);
		free(dataPageBuf);
		// �ر��ļ� CLOSE_FILE(fo);
		fclose(fo);

		return true;
	}
}

/* ����������Χɨ���ѯ���� */
void selectRangeData(unsigned int left, unsigned int right, vector<bool> field) {

	// ���벢��ʼ��ҳ MALLOC_PAGE(fileHeadBuf, File_head);
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head));
	memset(fileHeadBuf, 0, sizeof(File_head));
	// ���벢��ʼ��ҳ MALLOC_PAGE(dataPageBuf, Data_page);
	Data_page* dataPageBuf = (Data_page*)malloc(sizeof(Data_page));
	memset(dataPageBuf, 0, sizeof(Data_page));

	// ��mode��ʽ���ļ�,�������ļ�ͷҳ�� OPEN_FILE_READ("dataRecord.ibd", "rb+", fileHeadBuf, sizeof(File_head));
	// ��mode��ʽ���ļ� fo = OPEN_FILE("dataRecord.ibd", "rb+");
	fopen("dataRecord.ibd", "rb+");
	fread(fileHeadBuf, sizeof(File_head), 1, fo);

	Result result;
	searchDataPage(fileHeadBuf->rootPagePla, left, &result);

	// ָ��λ�ÿ�ʼ�� FSEEK_FIXED_READ(fo, result.pla, dataPageBuf, sizeof(Data_page));
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
				// ָ��λ�ÿ�ʼ�� FSEEK_FIXED_READ(fo, dataPageBuf->nextPagePla, dataPageBuf, sizeof(Data_page));
				fseek(fo, dataPageBuf->nextPagePla, SEEK_SET);
				fread(dataPageBuf, sizeof(Data_page), 1, fo);

				cur = 0;
			}
			else {
				break;
			}

		}
	}
	// �ͷ�ҳ FREE_PAGE(fileHeadBuf);
	free(fileHeadBuf);
	// �ͷ�ҳ FREE_PAGE(dataPageBuf);
	free(dataPageBuf);
	// �ر��ļ� CLOSE_FILE(fo);
	fclose(fo);
}

/* ����������Χɨ��Ĳ�ѯ�������� */
int outputCsvData(unsigned int left, unsigned int right, string csv_file_name) {
	ofstream outFile(csv_file_name, ios::out);
	// ���벢��ʼ��ҳ MALLOC_PAGE(fileHeadBuf, File_head);
	File_head* fileHeadBuf = (File_head*)malloc(sizeof(File_head));
	memset(fileHeadBuf, 0, sizeof(File_head));

	// ���벢��ʼ��ҳMALLOC_PAGE(dataPageBuf, Data_page);
	Data_page* dataPageBuf = (Data_page*)malloc(sizeof(Data_page));
	memset(dataPageBuf, 0, sizeof(Data_page));

	// ��mode��ʽ���ļ�,�������ļ�ͷҳ�� OPEN_FILE_READ("dataRecord.ibd", "rb+", fileHeadBuf, sizeof(File_head));
	// ��mode��ʽ���ļ� fo = OPEN_FILE("dataRecord.ibd", "rb+");
	fopen("dataRecord.ibd", "rb+");

	fread(fileHeadBuf, sizeof(File_head), 1, fo);

	Result result;
	searchDataPage(fileHeadBuf->rootPagePla, left, &result);

	// ָ��λ�ÿ�ʼ�� FSEEK_FIXED_READ(fo, result.pla, dataPageBuf, sizeof(Data_page));
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
				// ָ��λ�ÿ�ʼ�� FSEEK_FIXED_READ(fo, dataPageBuf->nextPagePla, dataPageBuf, sizeof(Data_page));
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
	// �ͷ�ҳ FREE_PAGE(fileHeadBuf);
	free(fileHeadBuf);
	// �ͷ�ҳ FREE_PAGE(dataPageBuf);
	free(dataPageBuf);
	// �ر��ļ� CLOSE_FILE(fo);
	fclose(fo);
	return cnt;
}