#include "stdafx.h"
#include "lock.h"
#import <msxml3.dll>
using namespace std;

extern ostringstream buffer;
void outputContent(bool error);

int statusXml(CSimpleIni &ini, const char *statusFlag)
{
	bool hasError = false;
	MSXML2::IXMLDOMDocumentPtr pXmlDom;
	HRESULT hr = pXmlDom.CreateInstance(__uuidof(MSXML2::DOMDocument30));
	if (FAILED(hr))
	{
		buffer << "Failed to CreateInstance on an XML DOM." << endl;
		outputContent(true);
		return -1;
	}
	pXmlDom->preserveWhiteSpace = VARIANT_FALSE;
	pXmlDom->async = VARIANT_FALSE;

	MSXML2::IXMLDOMProcessingInstructionPtr pi = pXmlDom->createProcessingInstruction("xml", "version=\"1.0\" encoding=\"gbk\"");
	if (pi != NULL) pXmlDom->appendChild(pi);

	MSXML2::IXMLDOMProcessingInstructionPtr pXslt = NULL;
	if(strcmp(statusFlag, "xml"))
	{
		pXslt = pXmlDom->createProcessingInstruction("xml-stylesheet", "type=\"text/xml\" href=\"../xslt/status.xsl\"");
		if (pXslt != NULL) pXmlDom->appendChild(pXslt);
	}

	MSXML2::IXMLDOMElementPtr root = pXmlDom->createNode(MSXML2::NODE_ELEMENT, "tdb_status", "");
	pXmlDom->appendChild(root);
	MSXML2::IXMLDOMElementPtr errorInfos = pXmlDom->createNode(MSXML2::NODE_ELEMENT, "error_infos", "");
	
	int licenseCount = -1;
	char lockData[16];
	int r = rand();
	memset(lockData, 0, sizeof(lockData));
	if(shieldPC(r) == Lock32_Function(r))
	{
		int operateResult = ReadLock(0, (unsigned char*)lockData, lock_passwd);
		if(operateResult == 0)
		{
			licenseCount = atoi(lockData);
			if(licenseCount <= 0 || licenseCount > 9999) licenseCount = -1;
		}
	}
	if(licenseCount == -1)
		sprintf_s(lockData, "%d", licenseCount);

	MSXML2::IXMLDOMElementPtr counter;
	counter = pXmlDom->createNode(MSXML2::NODE_ELEMENT, "license_counter", "");
	counter->appendChild(pXmlDom->createTextNode(lockData));
	root->appendChild(counter);

	CSimpleIni::TNamesDepend sections;
	ini.GetAllSections(sections);
	CSimpleIni::TNamesDepend::iterator sec = sections.begin();
	while(sec != sections.end())
	{
		string currentSection;
		const char *currentKey = NULL, *currentValue = NULL;
		try
		{
			currentSection = (*sec).pItem;
			if(string::npos != currentSection.find("PUBLISHER", 0) || currentSection == "TDB_INFO"
				|| currentSection == "ACTIVE_JOB" || currentSection == "COMPLETE_JOB")
			{
				MSXML2::IXMLDOMElementPtr sectionNode = pXmlDom->createNode(MSXML2::NODE_ELEMENT, currentSection.c_str(), "");
				CSimpleIni::TNamesDepend keys;
				ini.GetAllKeys((*sec).pItem, keys);
				CSimpleIni::TNamesDepend::iterator key = keys.begin();
				while(key != keys.end())
				{
					currentKey = (*key).pItem;
					currentValue = ini.GetValue(currentSection.c_str(), currentKey);
					MSXML2::IXMLDOMElementPtr item;
					if(currentSection == "ACTIVE_JOB" || currentSection == "COMPLETE_JOB")
					{
						item = pXmlDom->createNode(MSXML2::NODE_ELEMENT, "JOB", "");
						item->setAttribute("id", currentValue);
					}
					else
					{
						item = pXmlDom->createNode(MSXML2::NODE_ELEMENT, currentKey, "");
						item->appendChild(pXmlDom->createTextNode(currentValue));
					}
					sectionNode->appendChild(item);
					currentKey = NULL;
					currentValue = NULL;
					++key;
				}
				root->appendChild(sectionNode);
			}
			else  // treats it as job
			{
				MSXML2::IXMLDOMElementPtr sectionNode = pXmlDom->createNode(MSXML2::NODE_ELEMENT, "JOB_STATUS", "");
				sectionNode->setAttribute("id", currentSection.c_str());
				CSimpleIni::TNamesDepend keys;
				ini.GetAllKeys((*sec).pItem, keys);
				CSimpleIni::TNamesDepend::iterator key = keys.begin();
				while(key != keys.end())
				{
					currentKey = (*key).pItem;
					currentValue = ini.GetValue(currentSection.c_str(), currentKey);
					MSXML2::IXMLDOMElementPtr item = pXmlDom->createNode(MSXML2::NODE_ELEMENT, currentKey, "");
					item->appendChild(pXmlDom->createTextNode(currentValue));
					sectionNode->appendChild(item);
					currentKey = NULL;
					currentValue = NULL;
					++key;
				}
				root->appendChild(sectionNode);
			}
		}
		catch(_com_error &ex) 
		{
			hasError = true;
			ostringstream errbuf;
			errbuf << "�豸״̬����: 0x" << hex << ex.Error() << ',' << ex.ErrorMessage();
			if(! currentSection.empty())
			{
				errbuf << ", [" << currentSection  << ']';
				if(currentKey) errbuf << ", " << currentKey	<< " = " << (currentValue ? currentValue : "");
			}
			MSXML2::IXMLDOMElementPtr errorInfo = pXmlDom->createNode(MSXML2::NODE_ELEMENT, "error_info", "");
			errorInfo->appendChild(pXmlDom->createTextNode(errbuf.str().c_str()));
			errorInfos->appendChild(errorInfo);
		}
		++sec;
	}
	if(hasError) root->appendChild(errorInfos);
	buffer << "<?xml version=\"1.0\" encoding=\"gbk\"?>" << (pXslt ? pXslt->xml : "") << root->xml;
	outputContent(false);
	return 0;
}