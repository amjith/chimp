#include <check.h>

#include <chimp.h>

START_TEST(classes_should_be_callable)
{
    ChimpRef *instance;
    
    fail_unless (chimp_core_startup (&instance), "core_startup failed");

    instance = chimp_object_call (chimp_hash_class, chimp_array_new (NULL));
    fail_unless (instance != NULL,
                "failed to create 'hash' instance by calling the 'hash' "
                "class object.");
    fail_unless (chimp_object_instance_of (instance, chimp_hash_class),
                "expected new object to be an instance of the 'hash' class");

}
END_TEST

