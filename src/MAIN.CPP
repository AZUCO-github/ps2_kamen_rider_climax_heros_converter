///////////////////////////////////////////////////////////////////////////////
//	PS2 仮面ライダー クライマックスヒーローズ コンバータ
//			by	AZUCO
///////////////////////////////////////////////////////////////////////////////

#include	<windows.h>
#include	<stdio.h>
#include	"le.h"

extern	void	sector_read(			HANDLE,
										unsigned long,
										unsigned char *);
extern	void	convert_adpcm(
			unsigned long,
			unsigned long,
			unsigned char*,
			unsigned long,
			unsigned long				);


int	main(int argc,char *argv[])
{
	char			drive[MAX_PATH];
	char			dir[MAX_PATH];
	char			fname[MAX_PATH];
	char			ext[MAX_PATH];

	HANDLE			fhnd;
	LARGE_INTEGER	lifp;
	unsigned char	*bsrc=NULL;				//buffer src
	unsigned char	*bsce=NULL;				//buffer sce
	unsigned char	itbl[0x2000];			//item table

	unsigned long	ilsn;
	unsigned long	isiz;
	unsigned long	icnt;

	unsigned long	ssce;

	unsigned long	asiz;
	unsigned long	acnt;
	unsigned long	amax;
	unsigned long	atbl;
	unsigned long	aptr;
	unsigned long	sample_rate;

	unsigned long	fnargc=0;
	unsigned long	tul;

	printf("PS2 仮面ライダー クライマックスヒーローズ コンバータ\n");
	printf("\t\tProg. By AZUCO\n\n");

	if(argc==1){
		printf("mrch <option> <data file>\n");
		exit(-1);
	}

///////	option	///////////////////////////////////////////////////////////////
	for(tul=1;tul<argc;tul++){
		if(argv[tul][0]=='/'){
			if(	argv[tul][1]=='i'	)
				NULL;

		}else{
			if(fnargc==0)	fnargc=tul;
		}
	}

	fhnd=CreateFile((LPCSTR)argv[fnargc],
					GENERIC_READ,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
	if(fhnd==(HANDLE)-1){
		printf("\tError\tsource file not found\n");
		printf("\t%s\n",(LPCWSTR)argv[fnargc]);
		exit(-1);	}

	_splitpath(argv[fnargc],drive,dir,fname,ext);
	if(	strcmp(_strlwr(ext),".iso")==0){
		lifp.HighPart	=0x00000000;
		lifp.LowPart	=0x24a85d10;
		SetFilePointerEx(fhnd,lifp,NULL,FILE_BEGIN);
		ReadFile(fhnd,itbl,0x2000,&tul,NULL);

		for(icnt=0;icnt!=0x1f80;icnt+=8){
printf("%08d/%08d\n",icnt,0x1f80);
			ilsn=*(unsigned long *)(itbl+icnt+0);
			isiz=*(unsigned long *)(itbl+icnt+4);

			lifp.HighPart	=ilsn>>21;
			lifp.LowPart	=ilsn<<11;
			SetFilePointerEx(fhnd,lifp,NULL,FILE_BEGIN);
			bsrc=new unsigned char[isiz];
			ReadFile(fhnd,bsrc,isiz,&tul,NULL);

			switch(*(unsigned long *)bsrc){
				case	0x53434549:		//SCEI
/*
					sprintf(fname,"%08d-%08d.SCE",icnt,ilsn);
					ihnd=CreateFile(fname,GENERIC_WRITE,0,0,CREATE_ALWAYS,
												FILE_ATTRIBUTE_NORMAL,0);
					if(ihnd==(void *)-1)	printf("Can't create %s\n",fname);
					else{	WriteFile(ihnd,bsrc,isiz,&tul,NULL);
							CloseHandle(ihnd);								}
*/
/*
	SCEのデータを次のそれが出てくるまで保持する。
	これはSCEの中に次に出てくるSNDのサンプリングレート等の情報があるため。
	なので開放する場合は明示的にbsce=NULLとし、SCEの確保・開放を管理している。
	bsce!=0の時、SNDデコードでそれを参照する。
	無い場合はSNDの中にはひとつのADPCMストリームしかないと判断する
*/
					if(bsce!=NULL){
						delete[]	bsce;
						bsce=NULL;			}
					bsce=new unsigned char[isiz];
					memcpy(bsce,bsrc,isiz);
					ssce=isiz;

					goto	esc_b;
					break;

				case	0x54534452:		//TSDR
/*
					sprintf(fname,"%08d-%08d.TSD",icnt,ilsn);
					ihnd=CreateFile(fname,GENERIC_WRITE,0,0,CREATE_ALWAYS,
												FILE_ATTRIBUTE_NORMAL,0);
					if(ihnd==(void *)-1)	printf("Can't create %s\n",fname);
					else{	WriteFile(ihnd,bsrc,isiz,&tul,NULL);
							CloseHandle(ihnd);								}
*/
					goto	esc_b;
					break;
			}

			if(isiz==0)		goto	esc_b;
/*
					sprintf(fname,"%08d-%08d.SND",icnt,ilsn);
					ihnd=CreateFile(fname,GENERIC_WRITE,0,0,CREATE_ALWAYS,
												FILE_ATTRIBUTE_NORMAL,0);
					if(ihnd==(void *)-1)	printf("Can't create %s\n",fname);
					else{	WriteFile(ihnd,bsrc,isiz,&tul,NULL);
							CloseHandle(ihnd);									}
*/
/*
サンプルレートのわからないものに対して、BGMとわかっているものにだけ
44100を与える。後は22050とする。
*/
			if(bsce==NULL){
				switch(icnt){
					case	1416:
					case	1424:
					case	1432:
					case	1440:
					case	1448:
					case	1456:
					case	1464:
					case	1472:
					case	1480:
					case	1488:
					case	1496:
					case	1504:
					case	1512:
					case	1520:
						sample_rate=44100;
						break;
					default:
						sample_rate=22050;
						break;
				}
				convert_adpcm(icnt,0,bsrc,isiz,sample_rate);
				goto	esc_b;
			}

/*
SCEの処理。
SCEIVagiまでヘッダを進めて、テーブルのアイテムの個数と、テーブル位置を取得
テーブルからADPCMの実データの位置とサンプルレートを取得
サイズは次のデータの位置-現在のデータの位置から算出
*/
			atbl=0;									//IECSsreV
			atbl+=*(unsigned long *)(bsce+atbl+8);	//IECSdeaH
			atbl+=*(unsigned long *)(bsce+atbl+8);	//IECSigaV
			amax=*(unsigned long *)(bsce+atbl+0x0c)+1;
			atbl+=*(unsigned long *)(bsce+atbl+0x10);	//実際のテーブル

			for(acnt=0;acnt!=amax;acnt++){
				aptr		=*(unsigned long  *)(bsce+atbl+acnt*8+0);
				sample_rate	=*(unsigned short *)(bsce+atbl+acnt*8+4);
				if(acnt!=amax-1)
						asiz=*(unsigned long  *)(bsce+atbl+acnt*8+8);
				else	asiz=isiz;
				asiz-=aptr;
				convert_adpcm(icnt,acnt,bsrc+aptr,asiz,sample_rate);
			}

			if(bsce!=NULL){
				delete[]	bsce;
				bsce=NULL;			}

esc_b:
			if(bsrc!=NULL)	delete[]	bsrc;
		}
		CloseHandle(fhnd);
		goto	esc_a;
	}

esc_a:
	NULL;
}
