﻿#ifndef SQLITEDB_HPP_1C228281_1C8E_467F_9070_520E01F7DB70
#define SQLITEDB_HPP_1C228281_1C8E_467F_9070_520E01F7DB70
#pragma once

/*
sqlitedb.hpp

обёртка sqlite api для c++.
*/
/*
Copyright © 2011 Far Group
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "transactional.hpp"

namespace sqlite
{
	struct sqlite3;
	struct sqlite3_stmt;
}

class SQLiteDb: noncopyable, virtual transactional
{
public:
	SQLiteDb();
	virtual ~SQLiteDb() = default;

	enum class column_type
	{
		unknown,
		integer,
		string,
		blob,
	};

	bool IsNew() const { return db_exists <= 0; }
	int GetInitStatus(string& name, bool full_name) const;

	static int library_load();
	static void library_free();

protected:
	class SQLiteStmt
	{
	public:
		NONCOPYABLE(SQLiteStmt);
		TRIVIALLY_MOVABLE(SQLiteStmt);

		SQLiteStmt(sqlite::sqlite3_stmt* Stmt): m_Stmt(Stmt), m_Param(1) {}

		template<class T>
		struct transient_t
		{
			transient_t(const T& Value): m_Value(Value) {}
			const T& m_Value;
		};

		SQLiteStmt& Reset();
		bool Step() const;
		bool FinalStep() const;

		template<typename arg, typename... args>
		auto& Bind(arg&& Arg, args&&... Args)
		{
			return BindImpl(std::forward<arg>(Arg)), Bind(std::forward<args>(Args)...);
		}

		const wchar_t *GetColText(int Col) const;
		const char *GetColTextUTF8(int Col) const;
		int GetColInt(int Col) const;
		unsigned long long GetColInt64(int Col) const;
		blob_view GetColBlob(int Col) const;
		column_type GetColType(int Col) const;

	private:
		auto& Bind() { return *this; }

		SQLiteStmt& BindImpl(int Value);
		SQLiteStmt& BindImpl(long long Value);
		SQLiteStmt& BindImpl(const wchar_t* Value, bool bStatic = true);
		SQLiteStmt& BindImpl(const string& Value, bool bStatic = true);
		SQLiteStmt& BindImpl(string&& Value);
		SQLiteStmt& BindImpl(const blob_view& Value, bool bStatic = true);
		SQLiteStmt& BindImpl(unsigned int Value) { return BindImpl(static_cast<int>(Value)); }
		SQLiteStmt& BindImpl(unsigned long long Value) { return BindImpl(static_cast<long long>(Value)); }
		template<class T>
		SQLiteStmt& BindImpl(const transient_t<T>& Value) { return BindImpl(Value.m_Value, false); }

		struct stmt_deleter { void operator()(sqlite::sqlite3_stmt*) const; };
		std::unique_ptr<sqlite::sqlite3_stmt, stmt_deleter> m_Stmt;
		int m_Param;
	};

	struct statement_reset
	{
		void operator()(SQLiteStmt* Statement) const { Statement->Reset(); }
	};

	using auto_statement = std::unique_ptr<SQLiteStmt, statement_reset>;

	template<typename T>
	using stmt_init = std::pair<T, const wchar_t*>;

	template<class T>
	static auto transient(const T& Value) { return SQLiteStmt::transient_t<T>(Value); }

	bool Open(const string& DbName, bool Local, bool WAL=false);
	void Initialize(const string& DbName, bool Local = false);
	SQLiteStmt create_stmt(const wchar_t *Stmt) const;

	template<typename T, size_t N>
	bool PrepareStatements(const stmt_init<T> (&Init)[N])
	{
		TERSE_STATIC_ASSERT(N == T::stmt_count);

		assert(m_Statements.empty());

		m_Statements.reserve(N);
		std::transform(ALL_CONST_RANGE(Init), std::back_inserter(m_Statements), [this](const auto& i)
		{
			assert(i.first == m_Statements.size());
			// gcc bug, this-> required
			return this->create_stmt(i.second);
		});
		return true;
	}

	bool Exec(const char *Command) const;
	bool SetWALJournalingMode() const;
	bool EnableForeignKeysConstraints() const;
	bool Changes() const;
	unsigned long long LastInsertRowID() const;

	virtual bool BeginTransaction() override;
	virtual bool EndTransaction() override;
	virtual bool RollbackTransaction() override;

	// TODO: use in log
	int GetLastErrorCode() const;
	string GetLastErrorString() const;

	const string& GetPath() const { return m_Path; }
	const string& GetName() const { return m_Name; }

	auto AutoStatement(size_t Index) const { return auto_statement(&m_Statements[Index]); }

	template<typename... args>
	auto ExecuteStatement(size_t Index, args&&... Args) const
	{
		return AutoStatement(Index)->Bind(std::forward<args>(Args)...).FinalStep();
	}

private:
	void Close();
	virtual bool InitializeImpl(const string& DbName, bool Local) = 0;

	struct db_closer { void operator()(sqlite::sqlite3*) const; };
	using database_ptr = std::unique_ptr<sqlite::sqlite3, db_closer>;

	// must be destroyed last
	database_ptr m_Db;
	mutable std::vector<SQLiteStmt> m_Statements;
	string m_Path;
	string m_Name;
	int init_status;
	int db_exists;
};

#endif // SQLITEDB_HPP_1C228281_1C8E_467F_9070_520E01F7DB70
