#!/usr/bin/env python3
"""Build the self-contained project guide from Docs/Proje_Rehberi.md."""
from pathlib import Path
import re, html
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image, Preformatted, KeepTogether, CondPageBreak
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'Docs/Proje_Rehberi.md'
OUTPUT=ROOT/'output/pdf/Wireless_HE_Proje_Rehberi.pdf'
pdfmetrics.registerFont(TTFont('ArialTR','/System/Library/Fonts/Supplemental/Arial.ttf'))
pdfmetrics.registerFont(TTFont('ArialTR-Bold','/System/Library/Fonts/Supplemental/Arial Bold.ttf'))
pdfmetrics.registerFont(TTFont('CourierTR','/System/Library/Fonts/Supplemental/Courier New.ttf'))
pdfmetrics.registerFontFamily('ArialTR',normal='ArialTR',bold='ArialTR-Bold',italic='ArialTR',boldItalic='ArialTR-Bold')
styles={
 'title':ParagraphStyle('title',fontName='ArialTR-Bold',fontSize=23,leading=29,textColor=colors.HexColor('#122d28'),spaceAfter=12),
 'h':ParagraphStyle('h',fontName='ArialTR-Bold',fontSize=15,leading=20,textColor=colors.HexColor('#155c47'),spaceBefore=17,spaceAfter=7,keepWithNext=True),
 'p':ParagraphStyle('p',fontName='ArialTR',fontSize=9.3,leading=13.2,textColor=colors.HexColor('#243431'),spaceAfter=5),
 'bullet':ParagraphStyle('bullet',fontName='ArialTR',fontSize=9.4,leading=14,leftIndent=13,firstLineIndent=-9,textColor=colors.HexColor('#243431'),spaceAfter=4),
 'cell':ParagraphStyle('cell',fontName='ArialTR',fontSize=8.3,leading=11.2,textColor=colors.HexColor('#243431')),
 'head':ParagraphStyle('head',fontName='ArialTR-Bold',fontSize=8.5,leading=11,textColor=colors.white),
 'code':ParagraphStyle('code',fontName='CourierTR',fontSize=8,leading=11,leftIndent=8,rightIndent=8,backColor=colors.HexColor('#eff4f2'),borderPadding=8,spaceAfter=9,textColor=colors.HexColor('#1a3931')),
}
def esc(s):return html.escape(s,quote=False)
def footer(canvas,doc):
 canvas.saveState();w,h=A4;canvas.setStrokeColor(colors.HexColor('#d8e3de'));canvas.line(18*mm,16*mm,w-18*mm,16*mm)
 canvas.setFont('ArialTR',8);canvas.setFillColor(colors.HexColor('#61766d'));canvas.drawString(18*mm,11*mm,'WIRELESS HE  /  PROJE REHBERİ  /  05.11.2026');canvas.drawRightString(w-18*mm,11*mm,str(doc.page));canvas.restoreState()
def make_table(rows):
 data=[]
 for n,row in enumerate(rows):
  style=styles['head'] if n==0 else styles['cell']
  data.append([Paragraph(esc(cell),style) for cell in row])
 table=Table(data,colWidths=[62*mm,110*mm],repeatRows=1,hAlign='LEFT')
 table.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),colors.HexColor('#245f4c')),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.HexColor('#f2f6f4'),colors.white]),('VALIGN',(0,0),(-1,-1),'TOP'),('TOPPADDING',(0,0),(-1,-1),6),('BOTTOMPADDING',(0,0),(-1,-1),6),('LEFTPADDING',(0,0),(-1,-1),7),('RIGHTPADDING',(0,0),(-1,-1),7),('LINEBELOW',(0,0),(-1,0),0.4,colors.HexColor('#245f4c'))]))
 return table
lines=SOURCE.read_text().splitlines();story=[];i=0
while i<len(lines):
 line=lines[i].strip()
 if not line:i+=1;continue
 if line.startswith('# '):story.append(Paragraph(esc(line[2:]),styles['title']));i+=1;continue
 if line.startswith('## '):
  if line.startswith(('## 2. ', '## 4. ', '## 12. ', '## 13. ', '## 14. ')):story.append(CondPageBreak(120*mm))
  if line.startswith('## 16. '):story.append(CondPageBreak(80*mm))
  story.append(Paragraph(esc(line[3:]),styles['h']));i+=1;continue
 if line.startswith('!['):
  m=re.match(r'!\[(.*?)\]\((.*?)\)',line)
  if m:
   pic=Image(str((SOURCE.parent/m.group(2)).resolve()));ratio=pic.imageHeight/pic.imageWidth;pic.drawWidth=(170 if ratio<0.9 else 112)*mm;pic.drawHeight=pic.drawWidth*ratio;pic.hAlign='CENTER';story.extend([Spacer(1,4*mm),pic,Spacer(1,4*mm)])
  i+=1;continue
 if line.startswith('```'):
  i+=1;block=[]
  while i<len(lines) and not lines[i].startswith('```'):block.append(lines[i]);i+=1
  story.append(Preformatted('\n'.join(block),styles['code']));i+=1;continue
 if line.startswith('|'):
  rows=[]
  while i<len(lines) and lines[i].strip().startswith('|'):
   row=[v.strip() for v in lines[i].strip().strip('|').split('|')]
   if not all(re.fullmatch(r'[-: ]+',v or '-') for v in row):rows.append(row)
   i+=1
  story.append(KeepTogether([make_table(rows),Spacer(1,5*mm)]));continue
 if line.startswith('- ') or re.match(r'^\d+\. ',line):
  story.append(Paragraph('• '+esc(re.sub(r'^(?:- |\d+\. )','',line)),styles['bullet']));i+=1;continue
 story.append(Paragraph(esc(line),styles['p']));i+=1
OUTPUT.parent.mkdir(parents=True,exist_ok=True)
doc=SimpleDocTemplate(str(OUTPUT),pagesize=A4,leftMargin=19*mm,rightMargin=19*mm,topMargin=17*mm,bottomMargin=23*mm,title='Wireless HE Macropad - Bütünleşik Proje Rehberi',author='Wireless HE')
doc.build(story,onFirstPage=footer,onLaterPages=footer)
print(OUTPUT)
