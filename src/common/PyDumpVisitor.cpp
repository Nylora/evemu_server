/*
	------------------------------------------------------------------------------------
	LICENSE:
	------------------------------------------------------------------------------------
	This file is part of EVEmu: EVE Online Server Emulator
	Copyright 2006 - 2008 The EVEmu Team
	For the latest information visit http://evemu.mmoforge.org
	------------------------------------------------------------------------------------
	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU Lesser General Public License as published by the Free Software
	Foundation; either version 2 of the License, or (at your option) any later
	version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place - Suite 330, Boston, MA 02111-1307, USA, or go to
	http://www.gnu.org/copyleft/lesser.txt.
	------------------------------------------------------------------------------------
	Author:		Zhur
*/

#include "PyDumpVisitor.h"
#include "PyRep.h"
#include "misc.h"
#include "packet_functions.h"

PyDumpVisitor::PyDumpVisitor(bool full_lists) : PyVisitor(), m_full_lists(full_lists) { push("");}
PyDumpVisitor::~PyDumpVisitor() {}

PyLogsysDump::PyLogsysDump(LogType type, bool full_hex, bool full_lists)
: PyDumpVisitor(full_lists),
  m_type(type),
  m_hex_type(type),
  m_full_hex(full_hex)
{
}

PyLogsysDump::PyLogsysDump(LogType type, LogType hex_type, bool full_hex, bool full_lists)
: PyDumpVisitor(full_lists),
  m_type(type),
  m_hex_type(hex_type),
  m_full_hex(full_hex)
{
}

void PyLogsysDump::_print(const char *str, ...) {
	if(!is_log_enabled(m_type))
		return;	//calling log_message directly forces us to mask it
	va_list l;
	va_start(l, str);
	int len = strlen(top())+strlen(str)+2;
	char *buf = new char[len];
	snprintf(buf, len, "%s%s", top(), str);
	log_messageVA(m_type, buf, l);
	delete[] buf;
	va_end(l);
}

void PyLogsysDump::_hexDump(const uint8 *bytes, uint32 len) {
	if(m_full_hex)
		_hex(m_hex_type, bytes, len);
	else
		phex(m_hex_type, bytes, len);
}


PyFileDump::PyFileDump(FILE *into, bool full_hex)
: PyDumpVisitor(false),
  m_into(into),
  m_full_hex(full_hex)
{
}

void PyFileDump::_print(const char *str, ...) {
	va_list l;
	va_start(l, str);
	int len = strlen(top())+strlen(str)+2;
	char *buf = new char[len];
	snprintf(buf, len, "%s%s\n", top(), str);
	vfprintf(m_into, buf, l);
	delete[] buf;
	va_end(l);
}

void PyFileDump::_hexDump(const uint8 *bytes, uint32 len) {
	if(!m_full_hex && len > 1024) {
		char buffer[80];
		_pfxHexDump(bytes, 1024-32);
		fprintf(m_into, "%s ... truncated ...\n", top());
		build_hex_line((const char *)bytes, len, len-16, buffer, 4);
		fprintf(m_into, "%s%s\n", top(), buffer);
	} else {
		_pfxHexDump(bytes, len);
	}
}

void PyFileDump::_pfxHexDump(const uint8 *data, uint32 length) {
	char buffer[80];
	uint32 offset;
	for(offset=0;offset<length;offset+=16) {
		build_hex_line((const char *)data,length,offset,buffer,4);
		fprintf(m_into, "%s%s\n", top(), buffer);
	}
}








void PyDumpVisitor::VisitInteger(const PyRepInteger *rep) {
	_print("Integer field: %lld", rep->value);
}

void PyDumpVisitor::VisitReal(const PyRepReal *rep) {
	_print("Real Field: %f", rep->value);
}

void PyDumpVisitor::VisitBoolean(const PyRepBoolean *rep) {
	_print("Boolean field: %s", rep->value?"true":"false");
}

void PyDumpVisitor::VisitNone(const PyRepNone *rep) {
	_print("(None)");
}

void PyDumpVisitor::VisitBuffer(const PyRepBuffer *rep) {
	_print("Data buffer of length %d", rep->GetLength());

	//kinda hackish:
	if(rep->GetLength() > 2 && *(rep->GetBuffer()) == GZipStreamHeaderByte) {
		uint32 len = rep->GetLength();
		uint8 *buf = InflatePacket(rep->GetBuffer(), &len, true);
		if(buf != NULL) {
			std::string p(top());
			p += "  ";
			_print("  Data buffer contains gzipped data of length %lu", len);
			_hexDump(buf, len);
			delete[] buf;
		}
	} else if(rep->GetLength() > 0) {
		_hexDump(rep->GetBuffer(), rep->GetLength());
	}
}

void PyDumpVisitor::VisitNewObject(const PyRepNewObject *rep) {
	_print("%sNewObject:", top());
	PyVisitor::VisitNewObject(rep);
}

void PyDumpVisitor::VisitNewObjectHeader(const PyRepNewObject *rep) {
	_print("%sHeader:", top());
	PyVisitor::VisitNewObjectHeader(rep);
}

void PyDumpVisitor::VisitNewObjectList(const PyRepNewObject *rep) {
	_print("ListData: %lu entries", rep->list_data.size());
	PyVisitor::VisitNewObjectList(rep);
}

void PyDumpVisitor::VisitNewObjectListElement(const PyRepNewObject *rep, uint32 index, const PyRep *ele) {
	std::string n(top());
	{
		char t[16];
		snprintf(t, 16, "  [%2d] ", index);
		n += t;
	}
	push(n.c_str());
	PyVisitor::VisitNewObjectListElement(rep, index, ele);
	pop();
}

void PyDumpVisitor::VisitNewObjectDict(const PyRepNewObject *rep) {
	_print("DictData: %lu entries", rep->dict_data.size());
	PyVisitor::VisitNewObjectDict(rep);
}

void PyDumpVisitor::VisitNewObjectDictElement(const PyRepNewObject *rep, uint32 index, const PyRep *key, const PyRep *value) {
	std::string n(top());
	{
		char t[16];
		snprintf(t, 16, "  [%2d] Key: ", index);
		n += t;
	}
	push(n.c_str());
	key->visit(this);
	pop();

	n = top();
	{
		char t[16];
		snprintf(t, 16, "  [%2d] Value: ", index);
		n += t;
	}
	push(n.c_str());
	value->visit(this);
	pop();
}

void PyDumpVisitor::VisitPackedRow(const PyRepPackedRow *rep) {
	_print("Packed data of length %d", rep->bufferSize());
	
	std::string m(top());
	m += "  Header: ";
	push(m.c_str());
	rep->header->visit(this);
	pop();

	if(rep->bufferSize() > 0) {
		_hexDump(rep->buffer(), rep->bufferSize());
	}

	switch(rep->bufferSize()) {
	case 0x2b: {
		const uint8 *buf = rep->buffer();
		_print("  Len 0x2b decode:");
		_print("     u0: 0x%x", *buf); buf += sizeof(uint8);
		_print("     volRemaining: %.3f", *((const double *) buf) ); buf += sizeof(double);
		_print("     issued: " I64u , *((const uint64 *) buf) ); buf += sizeof(uint64);
		_print("     orderID: %lu", *((const uint32 *) buf) ); buf += sizeof(uint32);
		_print("     ???: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     u5: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     ???: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     u7: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     stationID: %lu", *((const uint32 *) buf) ); buf += sizeof(uint32);
		_print("     regionID: %lu", (*((const uint32 *) buf))&0xFFFFFF ); buf += sizeof(uint32)-sizeof(uint8);
		_print("     u10: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     solarSystemID: %lu", *((const uint32 *) buf) ); buf += sizeof(uint32);
		_print("     u12: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     typeID: %lu", *((const uint16 *) buf) ); buf += sizeof(uint16);
		_print("     jumps: %u (0x%x)", *((const uint16 *) buf), *((const uint16 *) buf) ); buf += sizeof(uint16);
		_print("     duration?: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
	} break;
	case 0x2c: {
		const uint8 *buf = rep->buffer();
		_print("  Len 0x2c decode:");
		_print("     volRemaining: %.3f", *((const double *) buf) ); buf += sizeof(double);
		_print("     issued: " I64u , *((const uint64 *) buf) ); buf += sizeof(uint64);
		_print("     orderID: %lu", *((const uint32 *) buf) ); buf += sizeof(uint32);
		_print("     ???: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     u5: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     ???: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     u7: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     stationID: %lu", *((const uint32 *) buf) ); buf += sizeof(uint32);
		_print("     regionID: %lu", (*((const uint32 *) buf))&0xFFFFFF ); buf += sizeof(uint32)-sizeof(uint8);
		_print("     u10: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     solarSystemID: %lu", *((const uint32 *) buf) ); buf += sizeof(uint32);
		_print("     u12: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     typeID: %lu", *((const uint16 *) buf) ); buf += sizeof(uint16);
		_print("     jumps: %u (0x%x)", *((const uint16 *) buf), *((const uint16 *) buf) ); buf += sizeof(uint16);
		_print("     duration?: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     u16: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     u17: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
	} break;
	case 0x2d: {
		const uint8 *buf = rep->buffer();
		_print("  Len 0x2d decode:");
		_print("     u0: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     volRemaining: %.3f", *((const double *) buf) ); buf += sizeof(double);
		_print("     issued: " I64u , *((const uint64 *) buf) ); buf += sizeof(uint64);
		_print("     orderID: %lu", *((const uint32 *) buf) ); buf += sizeof(uint32);
		_print("     ???: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     u5: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     ???: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     u7: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     stationID: %lu", *((const uint32 *) buf) ); buf += sizeof(uint32);
		_print("     regionID: %lu", (*((const uint32 *) buf))&0xFFFFFF ); buf += sizeof(uint32)-sizeof(uint8);
		_print("     u10: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     solarSystemID: %lu", *((const uint32 *) buf) ); buf += sizeof(uint32);
		_print("     u12: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     typeID: %lu", *((const uint16 *) buf) ); buf += sizeof(uint16);
		_print("     jumps: %u (0x%x)", *((const uint16 *) buf), *((const uint16 *) buf) ); buf += sizeof(uint16);
		_print("     duration?: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     u16: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
		_print("     u17: %d", *((const uint8 *) buf) ); buf += sizeof(uint8);
	} break;
	default:
		_print("  Unknown packed type");
	}
}

void PyDumpVisitor::VisitString(const PyRepString *rep) {
	if(ContainsNonPrintables(rep->value.c_str(), rep->value.length())) {
		_print("String%s: '<binary, len=%d>'", rep->is_type_1?" (Type1)":"", rep->value.length());
	} else {
		_print("String%s: '%s'", rep->is_type_1?" (Type1)":"", rep->value.c_str());
	}
}

	
void PyDumpVisitor::VisitObject(const PyRepObject *rep) {
	_print("Object:");
	_print("  Type: %s", rep->type.c_str());
	
	std::string m(top());
	m += "  Args: ";
	push(m.c_str());
	rep->arguments->visit(this);
	pop();
}
	
void PyDumpVisitor::VisitSubStruct(const PyRepSubStruct *rep) {
	_print("SubStruct: ");
	std::string indent(top());
	indent += "    ";
	push(indent.c_str());
	rep->sub->visit(this);
	pop();
}

void PyDumpVisitor::VisitSubStream(const PyRepSubStream *rep) {
	if(rep->decoded == NULL) {
		//we have not decoded this substream, leave it as hex:
		if(rep->data == NULL) {
			_print("INVALID Substream: no data (length %d)", rep->length);
		} else {
			_print("Substream: length %d", rep->length);
			std::string m(top());
			m += "  ";
			push(m.c_str());
			_hexDump(rep->data, rep->length);
			pop();
		}
	} else {
		_print("Substream: length %d %s", rep->length, (rep->data==NULL)?"from rep":"from data");
		std::string m(top());
		m += "  ";
		push(m.c_str());
		rep->decoded->visit(this);
		pop();
	}
}

void PyDumpVisitor::VisitDict(const PyRepDict *rep) {
	_print("Dictionary: %d entries", rep->items.size());
	PyVisitor::VisitDict(rep);
}

void PyDumpVisitor::VisitDictElement(const PyRepDict *rep, uint32 index, const PyRep *key, const PyRep *value) {
	std::string n(top());
	{
		char t[20];
		snprintf(t, sizeof(t), "  [%2ld] Key: ", index);
		n += t;
	}
	push(n.c_str());
	key->visit(this);
	pop();
	
	std::string m(top());
	{
		char t[20];
		snprintf(t, sizeof(t), "  [%2ld] Value: ", index);
		m += t;
	}
	push(m.c_str());
	value->visit(this);
	pop();
}

void PyDumpVisitor::VisitList(const PyRepList *rep) {
	if(rep->items.empty()) {
		_print("List: Empty");
	} else {
		_print("List: %d elements", rep->items.size());
		
		PyRepList::const_iterator cur, end;
		cur = rep->begin();
		end = rep->end();
		int r;
		for(r = 0; cur != end; cur++, r++) {
			if(!m_full_lists && r > 200) {
				_print("   ... truncated ...");
				break;
			}
			VisitListElement(rep, r, *cur);
		}
	}
}

void PyDumpVisitor::VisitListElement(const PyRepList *rep, uint32 index, const PyRep *ele) {
	std::string n(top());
	{
		char t[15];
		snprintf(t, 14, "  [%2ld] ", index);
		n += t;
	}
	push(n.c_str());
	PyVisitor::VisitListElement(rep, index, ele);
	pop();
}

void PyDumpVisitor::VisitTuple(const PyRepTuple *rep) {
	if(rep->items.empty())
		_print("Tuple: Empty");
	else {
		_print("Tuple: %d elements", rep->items.size());
		PyVisitor::VisitTuple(rep);
	}
}

void PyDumpVisitor::VisitTupleElement(const PyRepTuple *rep, uint32 index, const PyRep *ele) {
	std::string n(top());
	{
		char t[15];
		snprintf(t, 14, "  [%2ld] ", index);
		n += t;
	}
	push(n.c_str());
	PyVisitor::VisitTupleElement(rep, index, ele);
	pop();
}


void PyLogsysDump::VisitDict(const PyRepDict *rep) {
	//pre-mask potentially big operations
	if(is_log_enabled(m_type))
		PyDumpVisitor::VisitDict(rep);
}

void PyLogsysDump::VisitList(const PyRepList *rep) {
	//pre-mask potentially big operations
	if(is_log_enabled(m_type))
		PyDumpVisitor::VisitList(rep);
}

void PyLogsysDump::VisitTuple(const PyRepTuple *rep) {
	//pre-mask potentially big operations
	if(is_log_enabled(m_type))
		PyDumpVisitor::VisitTuple(rep);
}

void PyLogsysDump::VisitSubStream(const PyRepSubStream *rep) {
	//pre-mask potentially big operations
	if(is_log_enabled(m_type))
		PyDumpVisitor::VisitSubStream(rep);
}



















