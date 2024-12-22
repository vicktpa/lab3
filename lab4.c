MYSQL_LOCK *get_lock_data(THD *thd, TABLE **table_ptr, uint count, uint flags)
{
  uint i,lock_count,table_count;
  MYSQL_LOCK *sql_lock;
  THR_LOCK_DATA **locks, **locks_buf;
  TABLE **to, **table_buf;
  DBUG_ENTER("get_lock_data");

  DBUG_PRINT("info", ("count %d", count));

  for (i=lock_count=table_count=0 ; i < count ; i++)
  {
    TABLE *t= table_ptr[i];
    
    if ((likely(!t->s->tmp_table) ||
         (t->s->tmp_table == TRANSACTIONAL_TMP_TABLE)) &&
        (!(flags & GET_LOCK_SKIP_SEQUENCES) || t->s->sequence == 0))
    {
      lock_count+= t->file->lock_count();
      table_count++;
    }
  }

  /*
    Allocating twice the number of pointers for lock data for use in
    thr_multi_lock(). This function reorders the lock data, but cannot
    update the table values. So the second part of the array is copied
    from the first part immediately before calling thr_multi_lock().
  */
  size_t amount= sizeof(*sql_lock) +
                 sizeof(THR_LOCK_DATA*) * lock_count * 2 +
                 sizeof(table_ptr) * table_count;
  if (!(sql_lock= (MYSQL_LOCK*) (flags & GET_LOCK_ON_THD ?
                                 thd->alloc(amount) :
                                 my_malloc(key_memory_MYSQL_LOCK, amount,
                                           MYF(0)))))
    DBUG_RETURN(0);
  locks= locks_buf= sql_lock->locks= (THR_LOCK_DATA**) (sql_lock + 1);
  to= table_buf= sql_lock->table= (TABLE**) (locks + lock_count * 2);
  sql_lock->table_count= table_count;
  sql_lock->flags= flags;

  for (i=0 ; i < count ; i++)
  {
    TABLE *table= table_ptr[i];
    enum thr_lock_type lock_type;
    THR_LOCK_DATA **locks_start;

    if (!((likely(!table->s->tmp_table) ||
           (table->s->tmp_table == TRANSACTIONAL_TMP_TABLE)) &&
          (!(flags & GET_LOCK_SKIP_SEQUENCES) || table->s->sequence == 0)))
      continue;
    lock_type= table->reginfo.lock_type;
    DBUG_ASSERT(lock_type != TL_WRITE_DEFAULT && lock_type != TL_READ_DEFAULT);
    locks_start= locks;
    locks= table->file->store_lock(thd, locks,
             (flags & GET_LOCK_ACTION_MASK) == GET_LOCK_UNLOCK ? TL_IGNORE :
             lock_type);
    if ((flags & GET_LOCK_ACTION_MASK) == GET_LOCK_STORE_LOCKS)
    {
      table->lock_position=   (uint) (to - table_buf);
      table->lock_data_start= (uint) (locks_start - locks_buf);
      table->lock_count=      (uint) (locks - locks_start);
    }
    *to++= table;
    if (locks)
    {
      for ( ; locks_start != locks ; locks_start++)
      {
    (*locks_start)->debug_print_param= (void *) table;
        (*locks_start)->m_psi= table->file->m_psi;
    (*locks_start)->lock->name=         table->alias.c_ptr();
    (*locks_start)->org_type=           (*locks_start)->type;
      }
    }
  }
  /*
    We do not use 'lock_count', because there are cases where store_lock()
    returns less locks than lock_count() claimed. This can happen when
    a FLUSH TABLES tries to abort locks from a MERGE table of another
    thread. When that thread has just opened the table, but not yet
    attached its children, it cannot return the locks. lock_count()
    always returns the number of locks that an attached table has.
    This is done to avoid the reverse situation: If lock_count() would
    return 0 for a non-attached MERGE table, and that table becomes
    attached between the calls to lock_count() and store_lock(), then
    we would have allocated too little memory for the lock data. Now
    we may allocate too much, but better safe than memory overrun.
    And in the FLUSH case, the memory is released quickly anyway.
  */
  sql_lock->lock_count= (uint)(locks - locks_buf);
  DBUG_ASSERT(sql_lock->lock_count <= lock_count);
  DBUG_PRINT("info", ("sql_lock->table_count %d sql_lock->lock_count %d",
                      sql_lock->table_count, sql_lock->lock_count));
  DBUG_RETURN(sql_lock);
}

