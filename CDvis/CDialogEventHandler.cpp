#include "CDialogEventHandler.hpp"

#include <IOUtil.hpp>
#include <Graphics.hpp>
#include <Window.hpp>

// Indices of file types
#define INDEX_WORDDOC 1
#define INDEX_WEBPAGE 2
#define INDEX_TEXTDOC 3

// Controls
#define CONTROL_GROUP           2000
#define CONTROL_RADIOBUTTONLIST 2
#define CONTROL_RADIOBUTTON1    1
#define CONTROL_RADIOBUTTON2    2       // It is OK for this to have the same ID as CONTROL_RADIOBUTTONLIST,
										// because it is a child control under CONTROL_RADIOBUTTONLIST

// IDs for the Task Dialog Buttons
#define IDC_BASICFILEOPEN                       100
#define IDC_ADDITEMSTOCUSTOMPLACES              101
#define IDC_ADDCUSTOMCONTROLS                   102
#define IDC_SETDEFAULTVALUESFORPROPERTIES       103
#define IDC_WRITEPROPERTIESUSINGHANDLERS        104
#define IDC_WRITEPROPERTIESWITHOUTUSINGHANDLERS 105

HRESULT CDialogEventHandler::OnTypeChange(IFileDialog *pfd) {
	IFileSaveDialog *pfsd;
	HRESULT hr = pfd->QueryInterface(&pfsd);
	if (SUCCEEDED(hr)) {
		UINT uIndex;
		hr = pfsd->GetFileTypeIndex(&uIndex);   // index of current file-type
		if (SUCCEEDED(hr)) {
			IPropertyDescriptionList *pdl = NULL;

			switch (uIndex) {
			case INDEX_WORDDOC:
				// When .doc is selected, let's ask for some arbitrary property, say Title.
				hr = PSGetPropertyDescriptionListFromString(L"prop:System.Title", IID_PPV_ARGS(&pdl));
				if (SUCCEEDED(hr)) {
					// FALSE as second param == do not show default properties.
					hr = pfsd->SetCollectedProperties(pdl, FALSE);
					pdl->Release();
				}
				break;

			case INDEX_WEBPAGE:
				// When .html is selected, let's ask for some other arbitrary property, say Keywords.
				hr = PSGetPropertyDescriptionListFromString(L"prop:System.Keywords", IID_PPV_ARGS(&pdl));
				if (SUCCEEDED(hr)) {
					// FALSE as second param == do not show default properties.
					hr = pfsd->SetCollectedProperties(pdl, FALSE);
					pdl->Release();
				}
				break;

			case INDEX_TEXTDOC:
				// When .txt is selected, let's ask for some other arbitrary property, say Author.
				hr = PSGetPropertyDescriptionListFromString(L"prop:System.Author", IID_PPV_ARGS(&pdl));
				if (SUCCEEDED(hr)) {
					// TRUE as second param == show default properties as well, but show Author property first in list.
					hr = pfsd->SetCollectedProperties(pdl, TRUE);
					pdl->Release();
				}
				break;
			}
		}
		pfsd->Release();
	}
	return hr;
}

// IFileDialogControlEvents
// This method gets called when an dialog control item selection happens (radio-button selection. etc).
// For sample sake, let's react to this event by changing the dialog title.
HRESULT CDialogEventHandler::OnItemSelected(IFileDialogCustomize *pfdc, DWORD dwIDCtl, DWORD dwIDItem) {
	IFileDialog *pfd = NULL;
	HRESULT hr = pfdc->QueryInterface(&pfd);
	if (SUCCEEDED(hr)) {
		if (dwIDCtl == CONTROL_RADIOBUTTONLIST) {
			switch (dwIDItem) {
			case CONTROL_RADIOBUTTON1:
				hr = pfd->SetTitle(L"Longhorn Dialog");
				break;

			case CONTROL_RADIOBUTTON2:
				hr = pfd->SetTitle(L"Vista Dialog");
				break;
			}
		}
		pfd->Release();
	}
	return hr;
}

HRESULT CDialogEventHandler_CreateInstance(REFIID riid, void **ppv){
	*ppv = NULL;
	CDialogEventHandler *pDialogEventHandler = new (std::nothrow) CDialogEventHandler();
	HRESULT hr = pDialogEventHandler ? S_OK : E_OUTOFMEMORY;
	if (SUCCEEDED(hr)) {
		hr = pDialogEventHandler->QueryInterface(riid, ppv);
		pDialogEventHandler->Release();
	}
	return hr;
}

jwstring BrowseFile(){
	jwstring file;
	// CoCreate the File Open Dialog object.
	IFileDialog *pfd = NULL;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
	if (SUCCEEDED(hr)) {
		// Create an event handling object, and hook it up to the dialog.
		IFileDialogEvents *pfde = NULL;
		hr = CDialogEventHandler_CreateInstance(IID_PPV_ARGS(&pfde));
		if (SUCCEEDED(hr)) {
			// Hook up the event handler.
			DWORD dwCookie;
			hr = pfd->Advise(pfde, &dwCookie);
			if (SUCCEEDED(hr)) {
				// Set the options on the dialog.
				DWORD dwFlags;
				// Before setting, always get the options first in order 
				// not to override existing options.
				hr = pfd->GetOptions(&dwFlags);
				if (SUCCEEDED(hr)) {
					// In this case, get shell items only for file system items.
					hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
				}
				// Set the file types to display only. 
				// Notice that this is a 1-based array.
				static const COMDLG_FILTERSPEC filter[] = { L"All files (*.*)", L"*.*" };
				hr = pfd->SetFileTypes(1, filter);
				if (SUCCEEDED(hr)) {
					// Set the default extension to be ".doc" file.
					hr = pfd->SetDefaultExtension(L"*");
					//pfd->SetDefaultFolder({ initial });
					if (SUCCEEDED(hr)) {
						// Show the dialog
						hr = pfd->Show(Graphics::GetWindow()->GetHandle());
						if (SUCCEEDED(hr)) {
							// Obtain the result once the user clicks 
							// the 'Open' button.
							// The result is an IShellItem object.
							IShellItem *psiResult;
							hr = pfd->GetResult(&psiResult);
							if (SUCCEEDED(hr)) {
								// We are just going to print out the 
								// name of the file for sample sake.
								PWSTR pszFilePath = NULL;
								hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
								if (SUCCEEDED(hr)) {
									file = pszFilePath;
								}
								psiResult->Release();
							}
						}
					}
				}
				// Unhook the event handler.
				pfd->Unadvise(dwCookie);
			}
			pfde->Release();
		}
		pfd->Release();
	}
	return file;
}

jwstring BrowseFolder() {
	jwstring file;
	// CoCreate the File Open Dialog object.
	IFileDialog *pfd = NULL;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
	if (SUCCEEDED(hr)) {
		// Create an event handling object, and hook it up to the dialog.
		IFileDialogEvents *pfde = NULL;
		hr = CDialogEventHandler_CreateInstance(IID_PPV_ARGS(&pfde));
		if (SUCCEEDED(hr)) {
			// Hook up the event handler.
			DWORD dwCookie;
			hr = pfd->Advise(pfde, &dwCookie);
			if (SUCCEEDED(hr)) {
				// Set the options on the dialog.
				DWORD dwFlags;

				// Before setting, always get the options first in order 
				// not to override existing options.
				hr = pfd->GetOptions(&dwFlags);
				if (SUCCEEDED(hr)) {
					// In this case, get shell items only for file system items.
					pfd->SetOptions(dwFlags | FOS_PICKFOLDERS);
				}

				// Show the dialog
				hr = pfd->Show(Graphics::GetWindow()->GetHandle());
				if (SUCCEEDED(hr)) {
					// Obtain the result once the user clicks 
					// the 'Open' button.
					// The result is an IShellItem object.
					IShellItem *psiResult;
					hr = pfd->GetResult(&psiResult);
					if (SUCCEEDED(hr)) {
						// We are just going to print out the 
						// name of the file for sample sake.
						PWSTR pszFilePath = NULL;
						hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
						if (SUCCEEDED(hr))
							file = pszFilePath;
						psiResult->Release();
					}
				}
				// Unhook the event handler.
				pfd->Unadvise(dwCookie);
			}
			pfde->Release();
		}
		pfd->Release();
	}
	return file;
}