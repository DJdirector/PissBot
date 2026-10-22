# Each test is a small program that exits non-zero on failure.
# Label "unit" = no D++ needed (fast). Label "full" = needs D++.
function(pissbot_test name label)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS ${label})
endfunction()

pissbot_test(template_test unit pissbot_util)
pissbot_test(database_test unit pissbot_util)
pissbot_test(answers_test  unit pissbot_util)
pissbot_test(commands_test full pissbot_modules)
pissbot_test(access_test   full pissbot_modules)
