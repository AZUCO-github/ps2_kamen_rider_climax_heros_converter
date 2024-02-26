#include	<windows.h>
#include	<stdio.h>

		void	convert_adpcm(
			unsigned long,
			unsigned long,
			unsigned char*,
			unsigned long,
			unsigned long				);
		void	convert_adpcm_stereo(
			unsigned long,
			unsigned long,
			unsigned char*,
			unsigned long,
			unsigned long,
			unsigned long				);
		void	convert_adpcm_mono(
			unsigned long,
			unsigned long,
			unsigned char*,
			unsigned long,
			unsigned long				);



extern	void	decode_adpcm(			unsigned char *,
										short *,
										long *,
										long *			);

typedef	struct	{
	char	hriff[4];
	long	totalsize;

	char	hwave[8];
	long	hdrsize;
	short	format;
	short	channel;
	long	freq;
	long	byte_per_sec;
	short	blocksize;
	short	bits;

	char	hdata[4];
	long	datasize;
}	WAVHDR;



void	convert_adpcm(
	unsigned long	icnt,
	unsigned long	acnt,
	unsigned char	*src,
	unsigned long	ssiz,
	unsigned long	sample_rate	)
{
	unsigned long	a7ct;
	unsigned long	a7p1;
	unsigned long	a7p2;

/*
インターリーブ長のチェック＝モノラルかステレオかの判定
07でチェックできない場合は06でチェックする
同一フラグが複数出てくる場合があるので、最初とその次だけを記憶する
*/
		for(a7ct=1,a7p1=0,a7p2=0;a7ct<=ssiz;a7ct+=0x10){
			if(0x07 == *(unsigned char *)(src+a7ct))			//end code
				if(a7p1==0)		a7p1=a7ct;
				else
					if(a7p2==0)	a7p2=a7ct;
		}

	if((a7p1==0)&&(a7p2==0)){
		for(a7ct=1,a7p1=0,a7p2=0;a7ct<=ssiz;a7ct+=0x10){
			if(0x06 == *(unsigned char *)(src+a7ct))			//loop code ?
				if(a7p1==0)		a7p1=a7ct;
				else
					if(a7p2==0)	a7p2=a7ct;
		}
	}

	if(a7p2!=0)
			convert_adpcm_stereo(	icnt,acnt,src,ssiz,sample_rate,a7p2-a7p1);
	else	convert_adpcm_mono(		icnt,acnt,src,ssiz,sample_rate);
}



void	convert_adpcm_stereo(
	unsigned long	icnt,
	unsigned long	acnt,
	unsigned char	*src,
	unsigned long	ssiz,
	unsigned long	sample_rate,
	unsigned long	interleave_len	)
{
	unsigned long	sample_max;
	unsigned long	s,w;
	long			ppl,pl,ppr,pr;
	unsigned char	*srl;
	unsigned char	*srr;
	short			*wal;
	short			*war;
	short			*wav;
	WAVHDR			wavhdr;

	char			fname[MAX_PATH];
	HANDLE			ihnd;

	unsigned long	tul;

	sample_max=(ssiz>>5)*14*2;
	//byte size をパラグラフにして x 14 x short
	sprintf(wavhdr.hriff,	"RIFF");
	sprintf(wavhdr.hwave,	"WAVEfmt ");
	sprintf(wavhdr.hdata,	"data");
			wavhdr.hdrsize		=0x10;
			wavhdr.format		=1;		//PCM
			wavhdr.bits			=16;
			wavhdr.freq			=sample_rate;
			wavhdr.channel		=2;
			wavhdr.blocksize	=wavhdr.channel<<1;
			wavhdr.byte_per_sec =wavhdr.freq*wavhdr.blocksize;
			wavhdr.datasize		=wavhdr.blocksize*sample_max;
			wavhdr.totalsize	=wavhdr.datasize+sizeof(wavhdr)-8;

	srl=new unsigned char[ssiz];	//left
	srr=new unsigned char[ssiz];	//right
	wal=new short[sample_max*2];
	war=new short[sample_max*2];
	wav=new short[sample_max*wavhdr.channel*2];

	for(tul=0;tul<(ssiz>>1);tul+=interleave_len){	//インターリーブ解除
		memcpy(	srl+tul,
				src+tul*2,
				interleave_len);
		memcpy(	srr+tul,
				src+tul*2+interleave_len,
				interleave_len);
	}

	tul=ssiz>>5;						//Ｌチャネルデコード 4+1=/16/2
	s=0;	w=0;	ppl=0;	pl=0;
	while(tul) {
		decode_adpcm(srl+s,(short *)(wal+w),&ppl,&pl);
		s+=0x10;
		w+=14*2;
		tul--;											}

	tul=ssiz>>5;						//Ｒチャネル
	s=0;	w=0;	ppr=0;	pr=0;
	while(tul) {
		decode_adpcm(srr+s,(short *)(war+w),&ppr,&pr);
		s+=0x10;
		w+=14*2;
		tul--;											}

	for(tul=0;tul<sample_max;tul++){		//WAVインターリーブ
		wav[tul*2]		=wal[tul];
		wav[tul*2+1]	=war[tul];	}

	sprintf(fname,"%08d-%08d-stro.wav",icnt,acnt);
	ihnd=CreateFile(fname,GENERIC_WRITE,0,0,CREATE_ALWAYS,
								FILE_ATTRIBUTE_NORMAL,0);
	if(ihnd==(void *)-1){	printf("Can't create %s\n",fname);	}
	else{	WriteFile(ihnd,&wavhdr,sizeof(WAVHDR),&tul,NULL);
			WriteFile(ihnd,wav,sample_max*2*wavhdr.channel,&tul,NULL);
			CloseHandle(ihnd);					}

	if(srr!=NULL)	delete[]	srr;
	if(srl!=NULL)	delete[]	srl;
	if(war!=NULL)	delete[]	war;
	if(wal!=NULL)	delete[]	wal;
	if(wav!=NULL)	delete[]	wav;
}



void	convert_adpcm_mono(
	unsigned long	icnt,
	unsigned long	acnt,
	unsigned char	*src,
	unsigned long	ssiz,
	unsigned long	sample_rate		)
{
	unsigned long	sample_max;
	unsigned long	s,w;
	long			ppl,pl;
	short			*wav;
	WAVHDR			wavhdr;

	char			fname[MAX_PATH];
	HANDLE			ihnd;
	
	unsigned long	tul;

	sample_max=(ssiz>>4)*14*2;
	//byte size をパラグラフにして x 14 x short
	sprintf(wavhdr.hriff,	"RIFF");
	sprintf(wavhdr.hwave,	"WAVEfmt ");
	sprintf(wavhdr.hdata,	"data");
			wavhdr.hdrsize		=0x10;
			wavhdr.format		=1;		//PCM
			wavhdr.bits			=16;
			wavhdr.freq			=sample_rate;
			wavhdr.channel		=1;
			wavhdr.blocksize	=wavhdr.channel<<1;
			wavhdr.byte_per_sec =wavhdr.freq*wavhdr.blocksize;
			wavhdr.datasize		=wavhdr.blocksize*sample_max;
			wavhdr.totalsize	=wavhdr.datasize+sizeof(wavhdr)-8;

	wav=new short[sample_max*wavhdr.channel*2];

	tul=ssiz>>4;	//byte size をパラグラフに
	s=0;	w=0;	ppl=0;	pl=0;
	while(tul) {
		decode_adpcm(src+s,(short *)(wav+w),&ppl,&pl);
		s+=0x10;
		w+=14*2;
		tul--;										}

	sprintf(fname,"%08d-%08d-mono.wav",icnt,acnt);
	ihnd=CreateFile(fname,GENERIC_WRITE,0,0,CREATE_ALWAYS,
								FILE_ATTRIBUTE_NORMAL,0);
	if(ihnd==(void *)-1){	printf("Can't create %s\n",fname);	}
	else{	WriteFile(ihnd,&wavhdr,sizeof(WAVHDR),&tul,NULL);
			WriteFile(ihnd,wav,sample_max*2*wavhdr.channel,&tul,NULL);
			CloseHandle(ihnd);					}

	if(wav!=NULL)	delete[]	wav;
}