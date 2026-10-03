-- DESCRIPTION: Verilator: VHDL protected object changed by elaboration,
-- beside another object of its type
--
-- An object of a protected type whose method elaboration ran starts the run
-- with what that left in it (14.4.2). Its type is one class here, so with a
-- second object of the type, which starts fresh, that is refused.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_elabprot_unsup_pkg is
    type id_alloc_t is protected
        impure function next_id return integer;
    end protected;

    impure function first_id return integer;
    impure function other_id return integer;
end package;

package body t_vhdl_elabprot_unsup_pkg is
    type id_alloc_t is protected body
        variable counter : integer := 0;

        impure function next_id return integer is
        begin
            counter := counter + 1;
            return counter;
        end function;
    end protected body;

    shared variable id_alloc : id_alloc_t;
    shared variable other : id_alloc_t;

    impure function other_id return integer is
    begin
        return other.next_id;
    end function;

    impure function first_id return integer is
    begin
        return id_alloc.next_id;
    end function;
end package body;

-------------------------------------------------------------------------------

use work.t_vhdl_elabprot_unsup_pkg.all;

entity t_vhdl_elabprot_unsup is
end entity;

architecture test of t_vhdl_elabprot_unsup is
    constant n : integer := first_id;
    signal s : bit_vector(1 to n);
begin

    process is
    begin
        assert s'length = 1 severity failure;
        assert first_id = 2 severity failure;
        assert other_id = 1 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
