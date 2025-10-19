import { useEffect, useState } from "react";
import { supabase } from "@/integrations/supabase/client";
import { Tables } from "@/integrations/supabase/types";
import { useAuth } from "@/contexts/AuthContext";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import { Card } from "@/components/ui/card";
import { useToast } from "@/hooks/use-toast";
import { Label } from "@/components/ui/label";
import { Plus, Trash2 } from "lucide-react";
import { toast } from "sonner";

type Resume = Tables<"resume">;

const ResumeManagement = () => {
  const [resume, setResume] = useState<Resume | null>(null);
  const [formData, setFormData] = useState({
    full_name: "",
    email: "",
    phone: "",
    summary: "",
    skills: [] as string[],
    experience: [] as Array<{
      title: string;
      company: string;
      duration: string;
      description: string;
    }>,
    education: [] as Array<{
      degree: string;
      institution: string;
      year: string;
    }>,
    certifications: [] as string[],
  });
  const { user } = useAuth();
  const { toast } = useToast();

  useEffect(() => {
    fetchResume();
  }, []);

  const fetchResume = async () => {
    try {
      const { data, error } = await supabase
        .from("resume")
        .select("*")
        .maybeSingle();

      if (error) {
        console.error("Error fetching resume:", error);
        toast.error("Failed to fetch resume data");
        return;
      }

      if (data) {
        setResume(data);
        
        // Parse JSON fields safely
        const parseJsonField = (field: any, fallback: any) => {
          if (!field) return fallback;
          if (typeof field === 'string') {
            try {
              return JSON.parse(field);
            } catch {
              return fallback;
            }
          }
          return field;
        };

        // Extract skills from the complex JSON structure
        const skillsData = parseJsonField(data.skills, {});
        const allSkills = [
          ...(skillsData.technical || []),
          ...(skillsData.frameworks || []),
          ...(skillsData.tools || []),
          ...(skillsData.soft_skills || [])
        ];

        // Extract experience from JSON structure
        const experienceData = parseJsonField(data.experience, {});
        const positions = experienceData.positions || [];
        const simpleExperience = positions.map((pos: any) => ({
          title: pos.title || '',
          company: pos.company || '',
          duration: pos.duration || '',
          description: pos.responsibilities?.join(', ') || pos.description || ''
        }));

        // Extract education from JSON structure
        const educationData = parseJsonField(data.education, {});
        const degrees = educationData.degrees || [];
        const simpleEducation = degrees.map((deg: any) => ({
          degree: deg.degree || '',
          institution: deg.institution || '',
          year: deg.graduation_year?.toString() || deg.year || ''
        }));

        // Extract certifications
        const certificationsData = parseJsonField(data.certifications, {});
        const certs = certificationsData.certifications || [];
        const simpleCertifications = certs.map((cert: any) => 
          typeof cert === 'string' ? cert : cert.name || ''
        );

        setFormData({
          full_name: data.full_name || "",
          email: data.email || "",
          phone: data.phone || "",
          summary: data.summary || "",
          skills: allSkills,
          experience: simpleExperience,
          education: simpleEducation,
          certifications: simpleCertifications,
        });
      }
    } catch (error) {
      console.error("Error in fetchResume:", error);
      toast.error("Failed to load resume data");
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    const resumeData = {
      ...formData,
      user_id: user?.id,
    };

    if (resume) {
      const { error } = await supabase
        .from("resume")
        .update(resumeData)
        .eq("id", resume.id);

      if (error) {
        toast({ title: "Error updating resume", variant: "destructive" });
      } else {
        toast({ title: "Resume updated successfully" });
        fetchResume();
      }
    } else {
      const { error } = await supabase.from("resume").insert(resumeData);

      if (error) {
        toast({ title: "Error creating resume", variant: "destructive" });
      } else {
        toast({ title: "Resume created successfully" });
        fetchResume();
      }
    }
  };

  const addSkill = () => {
    setFormData({ ...formData, skills: [...formData.skills, ""] });
  };

  const updateSkill = (index: number, value: string) => {
    const newSkills = [...formData.skills];
    newSkills[index] = value;
    setFormData({ ...formData, skills: newSkills });
  };

  const removeSkill = (index: number) => {
    setFormData({
      ...formData,
      skills: formData.skills.filter((_, i) => i !== index),
    });
  };

  const addExperience = () => {
    setFormData({
      ...formData,
      experience: [
        ...formData.experience,
        { title: "", company: "", duration: "", description: "" },
      ],
    });
  };

  const updateExperience = (
    index: number,
    field: string,
    value: string
  ) => {
    const newExperience = [...formData.experience];
    newExperience[index] = { ...newExperience[index], [field]: value };
    setFormData({ ...formData, experience: newExperience });
  };

  const removeExperience = (index: number) => {
    setFormData({
      ...formData,
      experience: formData.experience.filter((_, i) => i !== index),
    });
  };

  const addEducation = () => {
    setFormData({
      ...formData,
      education: [...formData.education, { degree: "", institution: "", year: "" }],
    });
  };

  const updateEducation = (
    index: number,
    field: string,
    value: string
  ) => {
    const newEducation = [...formData.education];
    newEducation[index] = { ...newEducation[index], [field]: value };
    setFormData({ ...formData, education: newEducation });
  };

  const removeEducation = (index: number) => {
    setFormData({
      ...formData,
      education: formData.education.filter((_, i) => i !== index),
    });
  };

  const addCertification = () => {
    setFormData({ ...formData, certifications: [...formData.certifications, ""] });
  };

  const updateCertification = (index: number, value: string) => {
    const newCertifications = [...formData.certifications];
    newCertifications[index] = value;
    setFormData({ ...formData, certifications: newCertifications });
  };

  const removeCertification = (index: number) => {
    setFormData({
      ...formData,
      certifications: formData.certifications.filter((_, i) => i !== index),
    });
  };

  return (
    <Card className="p-6 glass">
      <h3 className="text-2xl font-bold mb-6">Edit Resume</h3>
      <form onSubmit={handleSubmit} className="space-y-6">
        <div className="grid gap-4 md:grid-cols-2">
          <div>
            <Label>Full Name</Label>
            <Input
              value={formData.full_name}
              onChange={(e) =>
                setFormData({ ...formData, full_name: e.target.value })
              }
              required
            />
          </div>
          <div>
            <Label>Email</Label>
            <Input
              type="email"
              value={formData.email}
              onChange={(e) =>
                setFormData({ ...formData, email: e.target.value })
              }
              required
            />
          </div>
          <div>
            <Label>Phone</Label>
            <Input
              value={formData.phone}
              onChange={(e) =>
                setFormData({ ...formData, phone: e.target.value })
              }
            />
          </div>
        </div>

        <div>
          <Label>Summary</Label>
          <Textarea
            value={formData.summary}
            onChange={(e) =>
              setFormData({ ...formData, summary: e.target.value })
            }
            rows={4}
          />
        </div>

        <div>
          <div className="flex justify-between items-center mb-2">
            <Label>Skills</Label>
            <Button type="button" size="sm" onClick={addSkill}>
              <Plus className="h-4 w-4 mr-1" /> Add Skill
            </Button>
          </div>
          {formData.skills.map((skill, index) => (
            <div key={index} className="flex gap-2 mb-2">
              <Input
                value={skill}
                onChange={(e) => updateSkill(index, e.target.value)}
              />
              <Button
                type="button"
                variant="destructive"
                size="icon"
                onClick={() => removeSkill(index)}
              >
                <Trash2 className="h-4 w-4" />
              </Button>
            </div>
          ))}
        </div>

        <div>
          <div className="flex justify-between items-center mb-2">
            <Label>Experience</Label>
            <Button type="button" size="sm" onClick={addExperience}>
              <Plus className="h-4 w-4 mr-1" /> Add Experience
            </Button>
          </div>
          {formData.experience.map((exp, index) => (
            <Card key={index} className="p-4 mb-4">
              <div className="space-y-2">
                <Input
                  placeholder="Job Title"
                  value={exp.title}
                  onChange={(e) =>
                    updateExperience(index, "title", e.target.value)
                  }
                />
                <Input
                  placeholder="Company"
                  value={exp.company}
                  onChange={(e) =>
                    updateExperience(index, "company", e.target.value)
                  }
                />
                <Input
                  placeholder="Duration"
                  value={exp.duration}
                  onChange={(e) =>
                    updateExperience(index, "duration", e.target.value)
                  }
                />
                <Textarea
                  placeholder="Description"
                  value={exp.description}
                  onChange={(e) =>
                    updateExperience(index, "description", e.target.value)
                  }
                />
                <Button
                  type="button"
                  variant="destructive"
                  size="sm"
                  onClick={() => removeExperience(index)}
                >
                  <Trash2 className="h-4 w-4 mr-1" /> Remove
                </Button>
              </div>
            </Card>
          ))}
        </div>

        <div>
          <div className="flex justify-between items-center mb-2">
            <Label>Education</Label>
            <Button type="button" size="sm" onClick={addEducation}>
              <Plus className="h-4 w-4 mr-1" /> Add Education
            </Button>
          </div>
          {formData.education.map((edu, index) => (
            <Card key={index} className="p-4 mb-4">
              <div className="space-y-2">
                <Input
                  placeholder="Degree"
                  value={edu.degree}
                  onChange={(e) =>
                    updateEducation(index, "degree", e.target.value)
                  }
                />
                <Input
                  placeholder="Institution"
                  value={edu.institution}
                  onChange={(e) =>
                    updateEducation(index, "institution", e.target.value)
                  }
                />
                <Input
                  placeholder="Year"
                  value={edu.year}
                  onChange={(e) =>
                    updateEducation(index, "year", e.target.value)
                  }
                />
                <Button
                  type="button"
                  variant="destructive"
                  size="sm"
                  onClick={() => removeEducation(index)}
                >
                  <Trash2 className="h-4 w-4 mr-1" /> Remove
                </Button>
              </div>
            </Card>
          ))}
        </div>

        <div>
          <div className="flex justify-between items-center mb-2">
            <Label>Certifications</Label>
            <Button type="button" size="sm" onClick={addCertification}>
              <Plus className="h-4 w-4 mr-1" /> Add Certification
            </Button>
          </div>
          {formData.certifications.map((cert, index) => (
            <div key={index} className="flex gap-2 mb-2">
              <Input
                value={cert}
                onChange={(e) => updateCertification(index, e.target.value)}
                placeholder="Certification name"
              />
              <Button
                type="button"
                variant="destructive"
                size="icon"
                onClick={() => removeCertification(index)}
              >
                <Trash2 className="h-4 w-4" />
              </Button>
            </div>
          ))}
        </div>

        <Button type="submit" className="w-full">
          Save Resume
        </Button>
      </form>
    </Card>
  );
};

export default ResumeManagement;
