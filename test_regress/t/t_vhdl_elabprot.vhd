-- DESCRIPTION: Verilator: VHDL protected objects elaboration calls (14.4.2)
--
-- The root architecture's constants are elaborated in order, once, before
-- the design runs, and an impure function they call may run a method of a
-- package's protected object: each constant is given what that call
-- returned, a generic of a record type is handed the constant, a type's
-- bound is the number its call gave, and the object starts the run with the
-- state elaboration left in it.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_elabprot_pkg is
    type rec_t is record
        id : integer;
        first : boolean;
    end record;

    type id_alloc_t is protected
        impure function next_id return integer;
    end protected;

    impure function next_rec return rec_t;
    impure function last_id return integer;
    function id_of (r : rec_t) return integer;
end package;

package body t_vhdl_elabprot_pkg is
    type id_alloc_t is protected body
        variable counter : integer := 0;

        impure function next_id return integer is
        begin
            counter := counter + 1;
            return counter;
        end function;
    end protected body;

    shared variable id_alloc : id_alloc_t;

    impure function last_id return integer is
    begin
        return id_alloc.next_id;
    end function;

    impure function next_rec return rec_t is
        variable id : integer;
    begin
        id := id_alloc.next_id;
        if id = 1 then
            return (id => id, first => true);
        end if;
        return (id => id, first => false);
    end function;

    function id_of (r : rec_t) return integer is
    begin
        return r.id;
    end function;
end package body;

-------------------------------------------------------------------------------

use work.t_vhdl_elabprot_pkg.all;

entity t_vhdl_elabprot_sub is
    generic ( r1, r2 : rec_t );
    port ( s1 : in bit_vector(1 to id_of(r1));
           s2 : in bit_vector(1 to id_of(r2)) );
end entity;

architecture test of t_vhdl_elabprot_sub is
begin
    process is
    begin
        assert r1.id = 1 and r1.first severity failure;
        assert r2.id = 2 and not r2.first severity failure;
        assert s1'length = 1 and s2'length = 2 severity failure;
        assert s1 = "1" and s2 = "01" severity failure;
        wait;
    end process;
end architecture;

-------------------------------------------------------------------------------

use work.t_vhdl_elabprot_pkg.all;

entity t_vhdl_elabprot is
end entity;

architecture test of t_vhdl_elabprot is
    constant r1, r2 : rec_t := next_rec;
    signal s1 : bit_vector(1 to 1) := "1";
    signal s2 : bit_vector(1 to 2) := "01";
    -- A type's bound is elaborated after them, once
    type arr_t is array (1 to 2) of bit_vector(last_id downto 0);
    signal a : arr_t;
    -- A signal's initial value is worked out when the design starts, from
    -- where elaboration left the object
    signal late : integer := last_id;
begin

    u : entity work.t_vhdl_elabprot_sub generic map ( r1, r2 ) port map ( s1, s2 );

    process is
    begin
        assert r1.id = 1 and r2.id = 2 severity failure;
        assert a(1)'length = 4 and a(2)'left = 3 severity failure;
        wait for 1 ns;
        -- The object carries on from where elaboration left it
        assert late = 4 severity failure;
        assert last_id = 5 severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
