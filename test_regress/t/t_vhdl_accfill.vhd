-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_accfill_pkg is
    type t_act is (INACTIVE, ACTIVE);
    type t_act_array is array (natural range <>) of t_act;
    type t_act_ptr is access t_act_array;
end package;

use work.t_vhdl_accfill_pkg.all;

entity t_vhdl_accfill is
end entity;

architecture test of t_vhdl_accfill is
begin

    main : process is
        variable n  : natural := 3;
        variable ap : t_act_ptr;
    begin
        ap := new t_act_array(0 to n - 1);

        -- An aggregate that gives every place the same value, written into
        -- what an access value points at: how many places there are is the
        -- object's own business (9.3.3.3)
        ap.all := (0 to n - 1 => ACTIVE);
        assert ap(0) = ACTIVE and ap(1) = ACTIVE and ap(2) = ACTIVE
            report "range choice" severity failure;

        ap.all := (0 to n - 1 => INACTIVE);
        assert ap(0) = INACTIVE and ap(2) = INACTIVE
            report "written again" severity failure;

        deallocate(ap);

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
