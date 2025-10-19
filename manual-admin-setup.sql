-- Manual Admin Setup SQL
-- Run this in your Supabase Dashboard > SQL Editor

-- First, let's see your user ID (run this first to get your user ID)
SELECT id, email FROM auth.users ORDER BY created_at DESC LIMIT 5;

-- Then, replace 'YOUR_USER_ID_HERE' with your actual user ID from above
-- and run this to add admin role:
INSERT INTO public.user_roles (user_id, role) 
VALUES ('YOUR_USER_ID_HERE', 'admin'::app_role)
ON CONFLICT (user_id, role) DO NOTHING;

-- Verify it worked:
SELECT ur.*, u.email 
FROM public.user_roles ur 
JOIN auth.users u ON ur.user_id = u.id;

-- If the above fails due to RLS, try this alternative approach:
-- Create a temporary function to bypass RLS (run as service role)
CREATE OR REPLACE FUNCTION public.setup_first_admin(target_email text)
RETURNS void
LANGUAGE plpgsql
SECURITY DEFINER
AS $$
DECLARE
    target_user_id uuid;
BEGIN
    -- Get user ID by email
    SELECT id INTO target_user_id 
    FROM auth.users 
    WHERE email = target_email;
    
    -- Insert admin role
    INSERT INTO public.user_roles (user_id, role)
    VALUES (target_user_id, 'admin'::app_role)
    ON CONFLICT (user_id, role) DO NOTHING;
END;
$$;

-- Use the function (replace with your email):
SELECT public.setup_first_admin('your-email@example.com');

-- Clean up the function:
DROP FUNCTION IF EXISTS public.setup_first_admin(text);
