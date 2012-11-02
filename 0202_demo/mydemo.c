int __int my_init_function(void)
{
    int err;

    /* 使用指针和名称注册 */
    err = register_this(ptr1, "skull");
    if (err) goto fail_this;
    err = register_that(ptr2, "skull");
    if (err) goto fail_that;
    err = register_those(ptr3, "skull");
    if (err) goto fail_those;

    return 0; /* 成功 */

    fail_those: unregister_that(ptr2, "skull");
    fail_that: unregister_this(ptr1, "skull");
    fail_this: return err; /* 返回错误 */
}

void __exit my_cleanup_function(void)
{
    unregister_those(ptr3, "skull");
    unregister_that(ptr2, "skull");
    unregister_this(ptr1, "skull");

    return;
}
